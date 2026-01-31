#include "Foundation/AxAllocatorAPI.h"
#include "Foundation/AxPlugin.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxMath.h"
#include "Foundation/AxPlatform.h"
#include "Foundation/AxArray.h"
#include "AxScene/AxScene.h"
#include "AxOpenGL/AxOpenGL.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

typedef struct AxSceneParser {
    const char* Source;
    size_t SourceLength;
    size_t CurrentPos;
    int CurrentLine;
    int CurrentColumn;
    AxToken CurrentToken;
    char ErrorMessage[256];
    struct AxAllocator* Allocator;
} AxSceneParser;

///////////////////////////////////////////////////////////////
// Global State and Configuration
///////////////////////////////////////////////////////////////

// Default memory size for scene allocators (1MB)
static size_t g_DefaultSceneMemorySize = 1024 * 1024;
static char g_LastErrorMessage[256] = {0};
static char g_ParserLastErrorMessage[256] = {0};

// Event Handler Chain Management
#define MAX_EVENT_HANDLERS 16
static AxSceneEvents g_EventHandlers[MAX_EVENT_HANDLERS];
static int g_EventHandlerCount = 0;

// API Handles
struct AxAllocatorAPI *AllocatorAPI;
struct AxPlatformAPI *PlatformAPI;

///////////////////////////////////////////////////////////////
// Error Handling
///////////////////////////////////////////////////////////////

static void SetError(const char* Format, ...)
{
    va_list Args;
    va_start(Args, Format);
    vsnprintf(g_LastErrorMessage, sizeof(g_LastErrorMessage), Format, Args);
    va_end(Args);
}

static void SetParserError(AxSceneParser* Parser, const char* Format, ...)
{
    va_list Args;
    va_start(Args, Format);
    vsnprintf(Parser->ErrorMessage, sizeof(Parser->ErrorMessage), Format, Args);
    va_end(Args);

    // Also copy to global error message
    strncpy(g_ParserLastErrorMessage, Parser->ErrorMessage, sizeof(g_ParserLastErrorMessage) - 1);
    g_ParserLastErrorMessage[sizeof(g_ParserLastErrorMessage) - 1] = '\0';
}

///////////////////////////////////////////////////////////////
// Event Handler Management
///////////////////////////////////////////////////////////////

static AxSceneResult RegisterEventHandler(const AxSceneEvents* Events)
{
    if (!Events) {
        SetError("Cannot register NULL event handler");
        return (AX_SCENE_ERROR_INVALID_ARGUMENTS);
    }

    if (g_EventHandlerCount >= MAX_EVENT_HANDLERS) {
        SetError("Maximum number of event handlers (%d) reached", MAX_EVENT_HANDLERS);
        return (AX_SCENE_ERROR_OUT_OF_MEMORY);
    }

    // Check for duplicate registration (compare by memory content)
    for (int i = 0; i < g_EventHandlerCount; i++) {
        if (memcmp(&g_EventHandlers[i], Events, sizeof(AxSceneEvents)) == 0) {
            SetError("Event handler already registered");
            return (AX_SCENE_ERROR_ALREADY_EXISTS);
        }
    }

    // Copy the event handler to our static array
    g_EventHandlers[g_EventHandlerCount] = *Events;
    g_EventHandlerCount++;

    return (AX_SCENE_SUCCESS);
}

static AxSceneResult UnregisterEventHandler(const AxSceneEvents* Events)
{
    if (!Events) {
        SetError("Cannot unregister NULL event handler");
        return (AX_SCENE_ERROR_INVALID_ARGUMENTS);
    }

    // Find the handler in our array (compare by memory content)
    for (int i = 0; i < g_EventHandlerCount; i++) {
        if (memcmp(&g_EventHandlers[i], Events, sizeof(AxSceneEvents)) == 0) {
            // Found it - shift remaining handlers down
            for (int j = i; j < g_EventHandlerCount - 1; j++) {
                g_EventHandlers[j] = g_EventHandlers[j + 1];
            }
            g_EventHandlerCount--;
            return (AX_SCENE_SUCCESS);
        }
    }

    SetError("Event handler not found in registry");
    return (AX_SCENE_ERROR_NOT_FOUND);
}

static void ClearEventHandlers(void)
{
    g_EventHandlerCount = 0;
    memset(g_EventHandlers, 0, sizeof(g_EventHandlers));
}

// Helper function to invoke callbacks for all registered handlers
static void InvokeCallbacks(void (*GetCallback)(const AxSceneEvents*, void**), const void* Object)
{
    for (int i = 0; i < g_EventHandlerCount; i++) {
        void (*Callback)(const void*, void*) = NULL;
        GetCallback(&g_EventHandlers[i], (void**)&Callback);

        if (Callback) {
            Callback(Object, g_EventHandlers[i].UserData);
        }
    }
}

// Callback getter helper functions
static void GetLightCallback(const AxSceneEvents* Events, void** OutCallback)
{
    *OutCallback = (void*)Events->OnLightParsed;
}

static void GetMaterialCallback(const AxSceneEvents* Events, void** OutCallback)
{
    *OutCallback = (void*)Events->OnMaterialParsed;
}

static void GetObjectCallback(const AxSceneEvents* Events, void** OutCallback)
{
    *OutCallback = (void*)Events->OnObjectParsed;
}

static void GetTransformCallback(const AxSceneEvents* Events, void** OutCallback)
{
    *OutCallback = (void*)Events->OnTransformParsed;
}

static void GetSceneCallback(const AxSceneEvents* Events, void** OutCallback)
{
    *OutCallback = (void*)Events->OnSceneParsed;
}

///////////////////////////////////////////////////////////////
// Scene Core Functionality
///////////////////////////////////////////////////////////////

static AxScene* CreateScene(const char* Name, size_t MemorySize)
{
    if (!Name || MemorySize == 0) {
        return (NULL);
    }

    // Create linear allocator for this scene using unified API
    struct AxAllocator* Allocator = AllocatorAPI->CreateLinear(Name, MemorySize);
    if (!Allocator) {
        return (NULL);
    }

    // Allocate scene structure from malloc (not from its own allocator to avoid corruption)
    AxScene* Scene = (AxScene*)malloc(sizeof(AxScene));
    if (!Scene) {
        Allocator->Destroy(Allocator);
        return (NULL);
    }

    // Initialize scene
    strncpy(Scene->Name, Name, sizeof(Scene->Name) - 1);
    Scene->Name[sizeof(Scene->Name) - 1] = '\0';
    Scene->RootObject = NULL;
    Scene->Materials = NULL;
    Scene->Lights = NULL;
    Scene->Cameras = NULL;
    Scene->CameraTransforms = NULL;
    Scene->ObjectCount = 0;
    Scene->MaterialCount = 0;
    Scene->LightCount = 0;
    Scene->CameraCount = 0;
    Scene->NextObjectID = 1; // Start IDs at 1 (0 reserved for invalid)
    Scene->Allocator = Allocator;

    return (Scene);
}

static void DestroyScene(AxScene* Scene)
{
    if (!Scene) {
        return;
    }

    // Destroying the allocator frees all scene memory at once
    if (Scene->Allocator) {
        Scene->Allocator->Destroy(Scene->Allocator);
    }

    // Free the scene structure itself (allocated with malloc)
    SAFE_FREE(Scene);
}

static AxSceneObject* CreateObject(AxScene* Scene, const char* Name, AxSceneObject* Parent)
{
    if (!Scene || !Name) {
        return (NULL);
    }

    // Allocate object from scene's allocator using unified interface
    AxSceneObject* Object = (AxSceneObject*)AxAlloc(Scene->Allocator, sizeof(AxSceneObject));
    if (!Object) {
        return (NULL);
    }

    // Initialize object
    strncpy(Object->Name, Name, sizeof(Object->Name) - 1);
    Object->Name[sizeof(Object->Name) - 1] = '\0';

    // Initialize transform to identity
    Object->Transform.Translation = (AxVec3){0.0f, 0.0f, 0.0f};
    Object->Transform.Rotation = (AxQuat){0.0f, 0.0f, 0.0f, 1.0f};
    Object->Transform.Scale = (AxVec3){1.0f, 1.0f, 1.0f};
    Object->Transform.Up = (AxVec3){0.0f, 1.0f, 0.0f};
    Object->Transform.Parent = NULL;
    Object->Transform.LastComputedFrame = 0;
    Object->Transform.ForwardMatrixDirty = true;
    Object->Transform.InverseMatrixDirty = true;
    Object->Transform.IsIdentity = true;

    // Clear asset paths
    Object->MeshPath[0] = '\0';
    Object->MaterialPath[0] = '\0';

    // Initialize hierarchy
    Object->Parent = NULL;
    Object->FirstChild = NULL;
    Object->NextSibling = NULL;
    Object->ObjectID = Scene->NextObjectID++;

    // Initialize runtime resource handle (not loaded)
    Object->LoadedModelIndex = 0;
    Object->LoadedModelGeneration = 0;

    // Link into hierarchy
    if (Parent) {
        Object->Parent = Parent;

        // Add as first child if parent has no children
        if (!Parent->FirstChild) {
            Parent->FirstChild = Object;
        } else {
            // Add as sibling to existing children
            AxSceneObject* Sibling = Parent->FirstChild;
            while (Sibling->NextSibling) {
                Sibling = Sibling->NextSibling;
            }
            Sibling->NextSibling = Object;
        }
    } else {
        // Root object - link as scene root if first, or as sibling
        if (!Scene->RootObject) {
            Scene->RootObject = Object;
        } else {
            // Add as sibling to existing root objects
            AxSceneObject* Sibling = Scene->RootObject;
            while (Sibling->NextSibling) {
                Sibling = Sibling->NextSibling;
            }
            Sibling->NextSibling = Object;
        }
    }

    Scene->ObjectCount++;
    return (Object);
}

static void SetObjectTransform(AxSceneObject* Object, const AxTransform* Transform)
{
    if (!Object || !Transform) {
        return;
    }

    Object->Transform = *Transform;
    Object->Transform.ForwardMatrixDirty = true;
    Object->Transform.InverseMatrixDirty = true;
    Object->Transform.IsIdentity = false;
}

static void SetObjectMesh(AxSceneObject* Object, const char* MeshPath)
{
    if (!Object || !MeshPath) {
        return;
    }

    strncpy(Object->MeshPath, MeshPath, sizeof(Object->MeshPath) - 1);
    Object->MeshPath[sizeof(Object->MeshPath) - 1] = '\0';
}

static void SetObjectMaterial(AxSceneObject* Object, const char* MaterialPath)
{
    if (!Object || !MaterialPath) {
        return;
    }

    strncpy(Object->MaterialPath, MaterialPath, sizeof(Object->MaterialPath) - 1);
    Object->MaterialPath[sizeof(Object->MaterialPath) - 1] = '\0';
}

// Helper function for recursive object search
static AxSceneObject* FindObjectRecursive(AxSceneObject* Object, const char* Name)
{
    if (!Object) {
        return (NULL);
    }

    // Check current object
    if (strcmp(Object->Name, Name) == 0) {
        return (Object);
    }

    // Search children
    AxSceneObject* Child = Object->FirstChild;
    while (Child) {
        AxSceneObject* Found = FindObjectRecursive(Child, Name);
        if (Found) {
            return (Found);
        }
        Child = Child->NextSibling;
    }

    return (NULL);
}

static AxSceneObject* FindObject(const AxScene* Scene, const char* Name)
{
    if (!Scene || !Name) {
        return (NULL);
    }

    // Search from all root objects
    AxSceneObject* Root = Scene->RootObject;
    while (Root) {
        AxSceneObject* Found = FindObjectRecursive(Root, Name);
        if (Found) {
            return (Found);
        }
        Root = Root->NextSibling;
    }

    return (NULL);
}

static void GetWorldTransform(AxSceneObject* Object, AxTransform* OutTransform)
{
    if (!Object || !OutTransform) {
        return;
    }

    // For now, just copy local transform
    // TODO: Implement proper hierarchy transform accumulation
    *OutTransform = Object->Transform;

    // Simple parent transform accumulation (basic implementation)
    if (Object->Parent) {
        AxTransform ParentWorld;
        GetWorldTransform(Object->Parent, &ParentWorld);

        // Combine transforms: ParentWorld * LocalTransform
        // For now, just accumulate translation (proper matrix math would be better)
        OutTransform->Translation.X += ParentWorld.Translation.X;
        OutTransform->Translation.Y += ParentWorld.Translation.Y;
        OutTransform->Translation.Z += ParentWorld.Translation.Z;
    }
}

///////////////////////////////////////////////////////////////
// Material Management Functions
///////////////////////////////////////////////////////////////

static struct AxMaterial* CreateMaterial(AxScene* Scene, const char* Name, const char* VertexShaderPath, const char* FragmentShaderPath)
{
    if (!Scene || !Name || !VertexShaderPath || !FragmentShaderPath) {
        return (NULL);
    }

    // Allocate or expand materials array
    if (!Scene->Materials) {
        Scene->Materials = (struct AxMaterial*)AxAlloc(Scene->Allocator, sizeof(struct AxMaterial));
        if (!Scene->Materials) {
            return (NULL);
        }
    } else {
        // For now, we'll use a simple approach and allocate a new array
        // TODO: Implement proper dynamic array growth
        struct AxMaterial* NewMaterials = (struct AxMaterial*)AxAlloc(
            Scene->Allocator,
            sizeof(struct AxMaterial) * (Scene->MaterialCount + 1)
        );
        if (!NewMaterials) {
            return (NULL);
        }

        // Copy existing materials
        for (uint32_t i = 0; i < Scene->MaterialCount; i++) {
            NewMaterials[i] = Scene->Materials[i];
        }
        Scene->Materials = NewMaterials;
    }

    // Initialize new material
    struct AxMaterial* Material = &Scene->Materials[Scene->MaterialCount];
    strncpy(Material->Name, Name, sizeof(Material->Name) - 1);
    Material->Name[sizeof(Material->Name) - 1] = '\0';
    strncpy(Material->VertexShaderPath, VertexShaderPath, sizeof(Material->VertexShaderPath) - 1);
    Material->VertexShaderPath[sizeof(Material->VertexShaderPath) - 1] = '\0';
    strncpy(Material->FragmentShaderPath, FragmentShaderPath, sizeof(Material->FragmentShaderPath) - 1);
    Material->FragmentShaderPath[sizeof(Material->FragmentShaderPath) - 1] = '\0';

    // Initialize other material properties
    Material->BaseColorFactor[0] = 1.0f;
    Material->BaseColorFactor[1] = 1.0f;
    Material->BaseColorFactor[2] = 1.0f;
    Material->BaseColorFactor[3] = 1.0f;
    Material->BaseColorTexture = 0;
    Material->NormalTexture = 0;

    // Initialize renderer-specific fields to prevent accessing garbage memory
    Material->ShaderProgram = 0;
    Material->ShaderData = NULL;

    Scene->MaterialCount++;
    return (Material);
}

static struct AxMaterial* FindMaterial(AxScene* Scene, const char* Name)
{
    if (!Scene || !Name || !Scene->Materials) {
        return (NULL);
    }

    for (uint32_t i = 0; i < Scene->MaterialCount; i++) {
        if (strcmp(Scene->Materials[i].Name, Name) == 0) {
            return (&Scene->Materials[i]);
        }
    }

    return (NULL);
}

///////////////////////////////////////////////////////////////
// Light Management Functions
///////////////////////////////////////////////////////////////

static AxLight* CreateLight(AxScene* Scene, const char* Name, AxLightType Type)
{
    if (!Scene || !Name) {
        return (NULL);
    }

    // Allocate space for new light (expand array if needed)
    if (Scene->Lights == NULL) {
        Scene->Lights = (AxLight*)AxAlloc(Scene->Allocator, sizeof(AxLight));
    } else {
        AxLight* NewLights = (AxLight*)AxAlloc(
            Scene->Allocator,
            sizeof(AxLight) * (Scene->LightCount + 1)
        );
        if (!NewLights) {
            return (NULL);
        }

        // Copy existing lights
        for (uint32_t i = 0; i < Scene->LightCount; i++) {
            NewLights[i] = Scene->Lights[i];
        }
        Scene->Lights = NewLights;
    }

    // Initialize new light
    AxLight* Light = &Scene->Lights[Scene->LightCount];
    strncpy(Light->Name, Name, sizeof(Light->Name) - 1);
    Light->Name[sizeof(Light->Name) - 1] = '\0';
    Light->Type = Type;
    Light->Position = (AxVec3){ 0.0f, 0.0f, 0.0f };
    Light->Direction = (AxVec3){ 0.0f, -1.0f, 0.0f }; // Default downward direction
    Light->Color = (AxVec3){ 1.0f, 1.0f, 1.0f }; // Default white light
    Light->Intensity = 1.0f;
    Light->Range = 0.0f; // Infinite range by default
    Light->InnerConeAngle = 0.5f; // ~28.6 degrees
    Light->OuterConeAngle = 1.0f; // ~57.3 degrees

    Scene->LightCount++;
    return (Light);
}

static AxSceneResult AddCamera(AxScene* Scene, const AxCamera* Camera, const AxTransform* Transform)
{
    if (!Scene || !Camera || !Transform) {
        return (AX_SCENE_ERROR_INVALID_ARGUMENTS);
    }

    printf("SceneAPI: AddCamera called - Camera FOV=%.2f, ptr=%p\n", Camera->FieldOfView, (void*)Camera);
    printf("SceneAPI: Scene->Allocator = %p\n", (void*)Scene->Allocator);

    // Check if allocator is valid
    if (!Scene->Allocator) {
        printf("SceneAPI: ERROR - Scene allocator is NULL!\n");
        return (AX_SCENE_ERROR_OUT_OF_MEMORY);
    }

    // Allocate space for new camera (expand arrays if needed)
    if (Scene->Cameras == NULL) {
        Scene->Cameras = (AxCamera*)AxAlloc(Scene->Allocator, sizeof(AxCamera));
        Scene->CameraTransforms = (AxTransform*)AxAlloc(Scene->Allocator, sizeof(AxTransform));
        printf("SceneAPI: Allocated new camera arrays - Cameras=%p, Transforms=%p\n",
               (void*)Scene->Cameras, (void*)Scene->CameraTransforms);

        if (!Scene->Cameras || !Scene->CameraTransforms) {
            printf("SceneAPI: ERROR - Allocator returned NULL!\n");
            return (AX_SCENE_ERROR_OUT_OF_MEMORY);
        }

        // Zero out the allocated memory
        memset(Scene->Cameras, 0, sizeof(AxCamera));
        memset(Scene->CameraTransforms, 0, sizeof(AxTransform));
    } else {
        // Allocate new arrays with space for one more camera
        AxCamera* NewCameras = (AxCamera*)AxAlloc(
            Scene->Allocator,
            sizeof(AxCamera) * (Scene->CameraCount + 1)
        );
        AxTransform* NewTransforms = (AxTransform*)AxAlloc(
            Scene->Allocator,
            sizeof(AxTransform) * (Scene->CameraCount + 1)
        );

        if (!NewCameras || !NewTransforms) {
            return (AX_SCENE_ERROR_OUT_OF_MEMORY);
        }

        // Copy existing cameras and transforms
        for (uint32_t i = 0; i < Scene->CameraCount; i++) {
            NewCameras[i] = Scene->Cameras[i];
            NewTransforms[i] = Scene->CameraTransforms[i];
        }

        Scene->Cameras = NewCameras;
        Scene->CameraTransforms = NewTransforms;
    }

    // Copy camera and transform to scene
    Scene->Cameras[Scene->CameraCount] = *Camera;
    Scene->CameraTransforms[Scene->CameraCount] = *Transform;

    printf("SceneAPI: Camera copied to index %u - FOV=%.2f, CameraPtr=%p\n",
           Scene->CameraCount, Scene->Cameras[Scene->CameraCount].FieldOfView,
           (void*)&Scene->Cameras[Scene->CameraCount]);

    Scene->CameraCount++;
    return (AX_SCENE_SUCCESS);
}

static AxCamera* GetCamera(const AxScene* Scene, uint32_t Index, AxTransform** OutTransform)
{
    if (!Scene || !Scene->Cameras || !Scene->CameraTransforms || Index >= Scene->CameraCount) {
        printf("SceneAPI: GetCamera failed - Scene=%p, Cameras=%p, Transforms=%p, Index=%u, Count=%u\n",
               (void*)Scene, (void*)(Scene ? Scene->Cameras : NULL),
               (void*)(Scene ? Scene->CameraTransforms : NULL),
               Index, Scene ? Scene->CameraCount : 0);
        return (NULL);
    }

    if (OutTransform) {
        *OutTransform = &Scene->CameraTransforms[Index];
    }

    return (&Scene->Cameras[Index]);
}

static AxLight* FindLight(AxScene* Scene, const char* Name)
{
    if (!Scene || !Name || !Scene->Lights) {
        return (NULL);
    }

    for (uint32_t i = 0; i < Scene->LightCount; i++) {
        if (strcmp(Scene->Lights[i].Name, Name) == 0) {
            return (&Scene->Lights[i]);
        }
    }

    return (NULL);
}

///////////////////////////////////////////////////////////////
// Parser Internal Functions
///////////////////////////////////////////////////////////////

static void InitParser(AxSceneParser* Parser, const char* Source, struct AxAllocator* Allocator);
static bool IsAtEnd(AxSceneParser* Parser);
static char CurrentChar(AxSceneParser* Parser);
static char PeekChar(AxSceneParser* Parser, int Offset);
static void AdvanceChar(AxSceneParser* Parser);
static void SkipWhitespace(AxSceneParser* Parser);
static void SkipComment(AxSceneParser* Parser);
static AxToken NextToken(AxSceneParser* Parser);
static AxToken ReadString(AxSceneParser* Parser);
static AxToken ReadNumber(AxSceneParser* Parser);
static AxToken ReadIdentifier(AxSceneParser* Parser);
static bool TokenEquals(AxToken* Token, const char* Expected);
static bool ExpectToken(AxSceneParser* Parser, AxTokenType Type);
static bool ParseFloat(AxToken* Token, float* Result);
static AxScene* ParseScene(AxSceneParser* Parser);
static AxSceneObject* ParseObject(AxSceneParser* Parser, AxScene* Scene, AxSceneObject* Parent);
static AxMaterial* ParseMaterial(AxSceneParser* Parser, AxScene* Scene);
static AxLight* ParseLight(AxSceneParser* Parser, AxScene* Scene);
static bool ParseTransform(AxSceneParser* Parser, AxTransform* Transform);
static bool ParseVector3(AxSceneParser* Parser, AxVec3* Vector);
static bool ParseQuaternion(AxSceneParser* Parser, AxQuat* Quaternion);
static void CopyTokenString(AxToken* Token, char* Dest, size_t DestSize);

static void InitParser(AxSceneParser* Parser, const char* Source, struct AxAllocator* Allocator)
{
    Parser->Source = Source;
    Parser->SourceLength = strlen(Source);
    Parser->CurrentPos = 0;
    Parser->CurrentLine = 1;
    Parser->CurrentColumn = 1;
    Parser->ErrorMessage[0] = '\0';
    Parser->Allocator = Allocator;
    Parser->CurrentToken = NextToken(Parser);
}

static bool IsAtEnd(AxSceneParser* Parser)
{
    return (Parser->CurrentPos >= Parser->SourceLength);
}

static char CurrentChar(AxSceneParser* Parser)
{
    if (IsAtEnd(Parser)) {
        return ('\0');
    }
    return (Parser->Source[Parser->CurrentPos]);
}

static char PeekChar(AxSceneParser* Parser, int Offset)
{
    size_t Pos = Parser->CurrentPos + Offset;
    if (Pos >= Parser->SourceLength) {
        return ('\0');
    }
    return (Parser->Source[Pos]);
}

static void AdvanceChar(AxSceneParser* Parser)
{
    if (!IsAtEnd(Parser)) {
        if (CurrentChar(Parser) == '\n') {
            Parser->CurrentLine++;
            Parser->CurrentColumn = 1;
        } else {
            Parser->CurrentColumn++;
        }
        Parser->CurrentPos++;
    }
}

static void SkipWhitespace(AxSceneParser* Parser)
{
    while (!IsAtEnd(Parser)) {
        char C = CurrentChar(Parser);
        if (C == ' ' || C == '\t' || C == '\r') {
            AdvanceChar(Parser);
        } else {
            break;
        }
    }
}

static void SkipComment(AxSceneParser* Parser)
{
    // Skip to end of line
    while (!IsAtEnd(Parser) && CurrentChar(Parser) != '\n') {
        AdvanceChar(Parser);
    }
}

static AxToken NextToken(AxSceneParser* Parser)
{
    SkipWhitespace(Parser);

    AxToken Token = {0};
    Token.Line = Parser->CurrentLine;
    Token.Column = Parser->CurrentColumn;
    Token.Start = &Parser->Source[Parser->CurrentPos];

    if (IsAtEnd(Parser)) {
        Token.Type = AX_TOKEN_EOF;
        Token.Length = 0;
        return (Token);
    }

    char C = CurrentChar(Parser);

    // Comments
    if (C == '#') {
        Token.Type = AX_TOKEN_COMMENT;
        SkipComment(Parser);
        Token.Length = &Parser->Source[Parser->CurrentPos] - Token.Start;
        return (Token);
    }

    // Newlines
    if (C == '\n') {
        Token.Type = AX_TOKEN_NEWLINE;
        Token.Length = 1;
        AdvanceChar(Parser);
        return (Token);
    }

    // Single character tokens
    switch (C) {
        case '{':
            Token.Type = AX_TOKEN_LBRACE;
            Token.Length = 1;
            AdvanceChar(Parser);
            return (Token);
        case '}':
            Token.Type = AX_TOKEN_RBRACE;
            Token.Length = 1;
            AdvanceChar(Parser);
            return (Token);
        case ':':
            Token.Type = AX_TOKEN_COLON;
            Token.Length = 1;
            AdvanceChar(Parser);
            return (Token);
        case ',':
            Token.Type = AX_TOKEN_COMMA;
            Token.Length = 1;
            AdvanceChar(Parser);
            return (Token);
    }

    // String literals
    if (C == '"') {
        return (ReadString(Parser));
    }

    // Numbers
    if (isdigit(C) || C == '-' || C == '+') {
        return (ReadNumber(Parser));
    }

    // Identifiers
    if (isalpha(C) || C == '_') {
        return (ReadIdentifier(Parser));
    }

    // Invalid character
    Token.Type = AX_TOKEN_INVALID;
    Token.Length = 1;
    AdvanceChar(Parser);
    return (Token);
}

static AxToken ReadString(AxSceneParser* Parser)
{
    AxToken Token = {0};
    Token.Type = AX_TOKEN_STRING;
    Token.Line = Parser->CurrentLine;
    Token.Column = Parser->CurrentColumn;
    Token.Start = &Parser->Source[Parser->CurrentPos];

    AdvanceChar(Parser); // Skip opening quote

    const char* StringStart = &Parser->Source[Parser->CurrentPos];

    while (!IsAtEnd(Parser) && CurrentChar(Parser) != '"') {
        if (CurrentChar(Parser) == '\n') {
            // Unterminated string
            Token.Type = AX_TOKEN_INVALID;
            return (Token);
        }
        AdvanceChar(Parser);
    }

    if (IsAtEnd(Parser)) {
        Token.Type = AX_TOKEN_INVALID;
        return (Token);
    }

    Token.Length = &Parser->Source[Parser->CurrentPos] - StringStart;
    Token.Start = StringStart; // Point to string content, not the quote

    AdvanceChar(Parser); // Skip closing quote
    return (Token);
}

static AxToken ReadNumber(AxSceneParser* Parser)
{
    AxToken Token = {0};
    Token.Type = AX_TOKEN_NUMBER;
    Token.Line = Parser->CurrentLine;
    Token.Column = Parser->CurrentColumn;
    Token.Start = &Parser->Source[Parser->CurrentPos];

    // Handle optional sign
    if (CurrentChar(Parser) == '-' || CurrentChar(Parser) == '+') {
        AdvanceChar(Parser);
    }

    // Read digits before decimal point
    while (!IsAtEnd(Parser) && isdigit(CurrentChar(Parser))) {
        AdvanceChar(Parser);
    }

    // Handle decimal point and fractional part
    if (!IsAtEnd(Parser) && CurrentChar(Parser) == '.') {
        AdvanceChar(Parser);
        while (!IsAtEnd(Parser) && isdigit(CurrentChar(Parser))) {
            AdvanceChar(Parser);
        }
    }

    Token.Length = &Parser->Source[Parser->CurrentPos] - Token.Start;
    return (Token);
}

static AxToken ReadIdentifier(AxSceneParser* Parser)
{
    AxToken Token = {0};
    Token.Type = AX_TOKEN_IDENTIFIER;
    Token.Line = Parser->CurrentLine;
    Token.Column = Parser->CurrentColumn;
    Token.Start = &Parser->Source[Parser->CurrentPos];

    while (!IsAtEnd(Parser)) {
        char C = CurrentChar(Parser);
        if (isalnum(C) || C == '_') {
            AdvanceChar(Parser);
        } else {
            break;
        }
    }

    Token.Length = &Parser->Source[Parser->CurrentPos] - Token.Start;
    return (Token);
}

static bool TokenEquals(AxToken* Token, const char* Expected)
{
    size_t ExpectedLen = strlen(Expected);
    return (Token->Length == ExpectedLen &&
            strncmp(Token->Start, Expected, Token->Length) == 0);
}

static bool ExpectToken(AxSceneParser* Parser, AxTokenType Type)
{
    // Skip comments and newlines
    while (Parser->CurrentToken.Type == AX_TOKEN_COMMENT ||
           Parser->CurrentToken.Type == AX_TOKEN_NEWLINE) {
        Parser->CurrentToken = NextToken(Parser);
    }

    return (Parser->CurrentToken.Type == Type);
}

static bool ParseFloat(AxToken* Token, float* Result)
{
    if (Token->Type != AX_TOKEN_NUMBER) {
        return (false);
    }

    char Buffer[32];
    size_t CopyLen = Token->Length;
    if (CopyLen >= sizeof(Buffer)) {
        CopyLen = sizeof(Buffer) - 1;
    }
    strncpy(Buffer, Token->Start, CopyLen);
    Buffer[CopyLen] = '\0';

    char* EndPtr;
    *Result = strtof(Buffer, &EndPtr);
    return (EndPtr != Buffer);
}

static void CopyTokenString(AxToken* Token, char* Dest, size_t DestSize)
{
    size_t CopyLen = Token->Length;
    if (CopyLen >= DestSize) {
        CopyLen = DestSize - 1;
    }
    strncpy(Dest, Token->Start, CopyLen);
    Dest[CopyLen] = '\0';
}

static bool ParseVector3(AxSceneParser* Parser, AxVec3* Vector)
{
    // Expect: x, y, z
    if (!ExpectToken(Parser, AX_TOKEN_NUMBER)) {
        SetParserError(Parser, "Expected number for X component at line %d", Parser->CurrentToken.Line);
        return (false);
    }
    if (!ParseFloat(&Parser->CurrentToken, &Vector->X)) {
        SetParserError(Parser, "Invalid number format for X component at line %d", Parser->CurrentToken.Line);
        return (false);
    }
    Parser->CurrentToken = NextToken(Parser);

    if (!ExpectToken(Parser, AX_TOKEN_COMMA)) {
        SetParserError(Parser, "Expected ',' after X component at line %d", Parser->CurrentToken.Line);
        return (false);
    }
    Parser->CurrentToken = NextToken(Parser);

    if (!ExpectToken(Parser, AX_TOKEN_NUMBER)) {
        SetParserError(Parser, "Expected number for Y component at line %d", Parser->CurrentToken.Line);
        return (false);
    }
    if (!ParseFloat(&Parser->CurrentToken, &Vector->Y)) {
        SetParserError(Parser, "Invalid number format for Y component at line %d", Parser->CurrentToken.Line);
        return (false);
    }
    Parser->CurrentToken = NextToken(Parser);

    if (!ExpectToken(Parser, AX_TOKEN_COMMA)) {
        SetParserError(Parser, "Expected ',' after Y component at line %d", Parser->CurrentToken.Line);
        return (false);
    }
    Parser->CurrentToken = NextToken(Parser);

    if (!ExpectToken(Parser, AX_TOKEN_NUMBER)) {
        SetParserError(Parser, "Expected number for Z component at line %d", Parser->CurrentToken.Line);
        return (false);
    }
    if (!ParseFloat(&Parser->CurrentToken, &Vector->Z)) {
        SetParserError(Parser, "Invalid number format for Z component at line %d", Parser->CurrentToken.Line);
        return (false);
    }
    Parser->CurrentToken = NextToken(Parser);

    return (true);
}

static bool ParseQuaternion(AxSceneParser* Parser, AxQuat* Quaternion)
{
    // Expect: x, y, z, w
    if (!ExpectToken(Parser, AX_TOKEN_NUMBER)) {
        SetParserError(Parser, "Expected number for quaternion X component at line %d", Parser->CurrentToken.Line);
        return (false);
    }
    if (!ParseFloat(&Parser->CurrentToken, &Quaternion->X)) {
        SetParserError(Parser, "Invalid number format for quaternion X component at line %d", Parser->CurrentToken.Line);
        return (false);
    }
    Parser->CurrentToken = NextToken(Parser);

    if (!ExpectToken(Parser, AX_TOKEN_COMMA)) {
        SetParserError(Parser, "Expected ',' after quaternion X component at line %d", Parser->CurrentToken.Line);
        return (false);
    }
    Parser->CurrentToken = NextToken(Parser);

    if (!ExpectToken(Parser, AX_TOKEN_NUMBER)) {
        SetParserError(Parser, "Expected number for quaternion Y component at line %d", Parser->CurrentToken.Line);
        return (false);
    }
    if (!ParseFloat(&Parser->CurrentToken, &Quaternion->Y)) {
        SetParserError(Parser, "Invalid number format for quaternion Y component at line %d", Parser->CurrentToken.Line);
        return (false);
    }
    Parser->CurrentToken = NextToken(Parser);

    if (!ExpectToken(Parser, AX_TOKEN_COMMA)) {
        SetParserError(Parser, "Expected ',' after quaternion Y component at line %d", Parser->CurrentToken.Line);
        return (false);
    }
    Parser->CurrentToken = NextToken(Parser);

    if (!ExpectToken(Parser, AX_TOKEN_NUMBER)) {
        SetParserError(Parser, "Expected number for quaternion Z component at line %d", Parser->CurrentToken.Line);
        return (false);
    }
    if (!ParseFloat(&Parser->CurrentToken, &Quaternion->Z)) {
        SetParserError(Parser, "Invalid number format for quaternion Z component at line %d", Parser->CurrentToken.Line);
        return (false);
    }
    Parser->CurrentToken = NextToken(Parser);

    if (!ExpectToken(Parser, AX_TOKEN_COMMA)) {
        SetParserError(Parser, "Expected ',' after quaternion Z component at line %d", Parser->CurrentToken.Line);
        return (false);
    }
    Parser->CurrentToken = NextToken(Parser);

    if (!ExpectToken(Parser, AX_TOKEN_NUMBER)) {
        SetParserError(Parser, "Expected number for quaternion W component at line %d", Parser->CurrentToken.Line);
        return (false);
    }
    if (!ParseFloat(&Parser->CurrentToken, &Quaternion->W)) {
        SetParserError(Parser, "Invalid number format for quaternion W component at line %d", Parser->CurrentToken.Line);
        return (false);
    }
    Parser->CurrentToken = NextToken(Parser);

    return (true);
}

static bool ParseTransform(AxSceneParser* Parser, AxTransform* Transform)
{
    // Initialize transform to identity
    Transform->Translation = (AxVec3){0.0f, 0.0f, 0.0f};
    Transform->Rotation = (AxQuat){0.0f, 0.0f, 0.0f, 1.0f};
    Transform->Scale = (AxVec3){1.0f, 1.0f, 1.0f};
    Transform->Up = (AxVec3){0.0f, 1.0f, 0.0f};
    Transform->Parent = NULL;
    Transform->LastComputedFrame = 0;
    Transform->ForwardMatrixDirty = true;
    Transform->InverseMatrixDirty = true;
    Transform->IsIdentity = false;

    if (!ExpectToken(Parser, AX_TOKEN_LBRACE)) {
        SetParserError(Parser, "Expected '{' to start transform block at line %d", Parser->CurrentToken.Line);
        return (false);
    }
    Parser->CurrentToken = NextToken(Parser);

    while (Parser->CurrentToken.Type != AX_TOKEN_RBRACE && Parser->CurrentToken.Type != AX_TOKEN_EOF) {
        if (!ExpectToken(Parser, AX_TOKEN_IDENTIFIER)) {
            SetParserError(Parser, "Expected transform property name at line %d", Parser->CurrentToken.Line);
            return (false);
        }

        if (TokenEquals(&Parser->CurrentToken, "translation")) {
            Parser->CurrentToken = NextToken(Parser);
            if (!ExpectToken(Parser, AX_TOKEN_COLON)) {
                SetParserError(Parser, "Expected ':' after 'translation' at line %d", Parser->CurrentToken.Line);
                return (false);
            }
            Parser->CurrentToken = NextToken(Parser);
            if (!ParseVector3(Parser, &Transform->Translation)) {
                return (false);
            }
        } else if (TokenEquals(&Parser->CurrentToken, "rotation")) {
            Parser->CurrentToken = NextToken(Parser);
            if (!ExpectToken(Parser, AX_TOKEN_COLON)) {
                SetParserError(Parser, "Expected ':' after 'rotation' at line %d", Parser->CurrentToken.Line);
                return (false);
            }
            Parser->CurrentToken = NextToken(Parser);
            if (!ParseQuaternion(Parser, &Transform->Rotation)) {
                return (false);
            }
        } else if (TokenEquals(&Parser->CurrentToken, "scale")) {
            Parser->CurrentToken = NextToken(Parser);
            if (!ExpectToken(Parser, AX_TOKEN_COLON)) {
                SetParserError(Parser, "Expected ':' after 'scale' at line %d", Parser->CurrentToken.Line);
                return (false);
            }
            Parser->CurrentToken = NextToken(Parser);
            if (!ParseVector3(Parser, &Transform->Scale)) {
                return (false);
            }
        } else {
            SetParserError(Parser, "Unknown transform property at line %d", Parser->CurrentToken.Line);
            return (false);
        }

        // Skip optional newlines and comments
        while (Parser->CurrentToken.Type == AX_TOKEN_COMMENT ||
               Parser->CurrentToken.Type == AX_TOKEN_NEWLINE) {
            Parser->CurrentToken = NextToken(Parser);
        }
    }

    if (!ExpectToken(Parser, AX_TOKEN_RBRACE)) {
        SetParserError(Parser, "Expected '}' to close transform block at line %d", Parser->CurrentToken.Line);
        return (false);
    }
    Parser->CurrentToken = NextToken(Parser);

    return (true);
}

static AxSceneObject* ParseObject(AxSceneParser* Parser, AxScene* Scene, AxSceneObject* Parent)
{
    if (!ExpectToken(Parser, AX_TOKEN_IDENTIFIER)) {
        SetParserError(Parser, "Expected 'object' keyword at line %d", Parser->CurrentToken.Line);
        return (NULL);
    }

    if (!TokenEquals(&Parser->CurrentToken, "object")) {
        SetParserError(Parser, "Expected 'object' keyword at line %d", Parser->CurrentToken.Line);
        return (NULL);
    }
    Parser->CurrentToken = NextToken(Parser);

    if (!ExpectToken(Parser, AX_TOKEN_STRING)) {
        SetParserError(Parser, "Expected object name string at line %d", Parser->CurrentToken.Line);
        return (NULL);
    }

    char ObjectName[64];
    CopyTokenString(&Parser->CurrentToken, ObjectName, sizeof(ObjectName));
    Parser->CurrentToken = NextToken(Parser);

    if (!ExpectToken(Parser, AX_TOKEN_LBRACE)) {
        SetParserError(Parser, "Expected '{' to start object block at line %d", Parser->CurrentToken.Line);
        return (NULL);
    }
    Parser->CurrentToken = NextToken(Parser);

    // Create the scene object
    AxSceneObject* Object = CreateObject(Scene, ObjectName, Parent);
    if (!Object) {
        SetParserError(Parser, "Failed to create scene object '%s' (Scene=%p, Parent=%p)", ObjectName, Scene, Parent);
        return (NULL);
    }

    while (Parser->CurrentToken.Type != AX_TOKEN_RBRACE && Parser->CurrentToken.Type != AX_TOKEN_EOF) {
        if (!ExpectToken(Parser, AX_TOKEN_IDENTIFIER)) {
            SetParserError(Parser, "Expected property name or 'object' at line %d", Parser->CurrentToken.Line);
            return (NULL);
        }

        if (TokenEquals(&Parser->CurrentToken, "transform")) {
            Parser->CurrentToken = NextToken(Parser);
            if (!ParseTransform(Parser, &Object->Transform)) {
                return (NULL);
            }
            // Update transform properties
            Object->Transform.ForwardMatrixDirty = true;
            Object->Transform.InverseMatrixDirty = true;
            Object->Transform.IsIdentity = false;

            // Trigger OnTransformParsed callback
            InvokeCallbacks(GetTransformCallback, &Object->Transform);
        } else if (TokenEquals(&Parser->CurrentToken, "mesh")) {
            Parser->CurrentToken = NextToken(Parser);
            if (!ExpectToken(Parser, AX_TOKEN_COLON)) {
                SetParserError(Parser, "Expected ':' after 'mesh' at line %d", Parser->CurrentToken.Line);
                return (NULL);
            }
            Parser->CurrentToken = NextToken(Parser);
            if (!ExpectToken(Parser, AX_TOKEN_STRING)) {
                SetParserError(Parser, "Expected mesh path string at line %d", Parser->CurrentToken.Line);
                return (NULL);
            }
            CopyTokenString(&Parser->CurrentToken, Object->MeshPath, sizeof(Object->MeshPath));
            Parser->CurrentToken = NextToken(Parser);
        } else if (TokenEquals(&Parser->CurrentToken, "material")) {
            Parser->CurrentToken = NextToken(Parser);
            if (!ExpectToken(Parser, AX_TOKEN_COLON)) {
                SetParserError(Parser, "Expected ':' after 'material' at line %d", Parser->CurrentToken.Line);
                return (NULL);
            }
            Parser->CurrentToken = NextToken(Parser);
            if (!ExpectToken(Parser, AX_TOKEN_STRING)) {
                SetParserError(Parser, "Expected material path string at line %d", Parser->CurrentToken.Line);
                return (NULL);
            }
            CopyTokenString(&Parser->CurrentToken, Object->MaterialPath, sizeof(Object->MaterialPath));
            Parser->CurrentToken = NextToken(Parser);
        } else if (TokenEquals(&Parser->CurrentToken, "object")) {
            // Parse child object
            AxSceneObject* ChildObject = ParseObject(Parser, Scene, Object);
            if (!ChildObject) {
                return (NULL);
            }
        } else {
            SetParserError(Parser, "Unknown object property '%.*s' at line %d",
                    (int)Parser->CurrentToken.Length, Parser->CurrentToken.Start, Parser->CurrentToken.Line);
            return (NULL);
        }

        // Skip optional newlines and comments
        while (Parser->CurrentToken.Type == AX_TOKEN_COMMENT ||
               Parser->CurrentToken.Type == AX_TOKEN_NEWLINE) {
            Parser->CurrentToken = NextToken(Parser);
        }
    }

    if (!ExpectToken(Parser, AX_TOKEN_RBRACE)) {
        SetParserError(Parser, "Expected '}' to close object block at line %d", Parser->CurrentToken.Line);
        return (NULL);
    }
    Parser->CurrentToken = NextToken(Parser);

    // Trigger OnObjectParsed callback
    InvokeCallbacks(GetObjectCallback, Object);

    return (Object);
}

static AxMaterial* ParseMaterial(AxSceneParser* Parser, AxScene* Scene)
{
    if (!ExpectToken(Parser, AX_TOKEN_IDENTIFIER)) {
        SetParserError(Parser, "Expected 'material' keyword at line %d", Parser->CurrentToken.Line);
        return (NULL);
    }

    if (!TokenEquals(&Parser->CurrentToken, "material")) {
        SetParserError(Parser, "Expected 'material' keyword at line %d", Parser->CurrentToken.Line);
        return (NULL);
    }
    Parser->CurrentToken = NextToken(Parser);

    if (!ExpectToken(Parser, AX_TOKEN_STRING)) {
        SetParserError(Parser, "Expected material name string at line %d", Parser->CurrentToken.Line);
        return (NULL);
    }

    char MaterialName[64];
    CopyTokenString(&Parser->CurrentToken, MaterialName, sizeof(MaterialName));
    Parser->CurrentToken = NextToken(Parser);

    if (!ExpectToken(Parser, AX_TOKEN_LBRACE)) {
        SetParserError(Parser, "Expected '{' to start material block at line %d", Parser->CurrentToken.Line);
        return (NULL);
    }
    Parser->CurrentToken = NextToken(Parser);

    char VertexShaderPath[256] = "";
    char FragmentShaderPath[256] = "";

    while (Parser->CurrentToken.Type != AX_TOKEN_RBRACE && Parser->CurrentToken.Type != AX_TOKEN_EOF) {
        if (!ExpectToken(Parser, AX_TOKEN_IDENTIFIER)) {
            SetParserError(Parser, "Expected property name at line %d", Parser->CurrentToken.Line);
            return (NULL);
        }

        if (TokenEquals(&Parser->CurrentToken, "vertexShader")) {
            Parser->CurrentToken = NextToken(Parser);
            if (!ExpectToken(Parser, AX_TOKEN_COLON)) {
                SetParserError(Parser, "Expected ':' after 'vertexShader' at line %d", Parser->CurrentToken.Line);
                return (NULL);
            }
            Parser->CurrentToken = NextToken(Parser);
            if (!ExpectToken(Parser, AX_TOKEN_STRING)) {
                SetParserError(Parser, "Expected vertex shader path string at line %d", Parser->CurrentToken.Line);
                return (NULL);
            }
            CopyTokenString(&Parser->CurrentToken, VertexShaderPath, sizeof(VertexShaderPath));
            Parser->CurrentToken = NextToken(Parser);
        } else if (TokenEquals(&Parser->CurrentToken, "fragmentShader")) {
            Parser->CurrentToken = NextToken(Parser);
            if (!ExpectToken(Parser, AX_TOKEN_COLON)) {
                SetParserError(Parser, "Expected ':' after 'fragmentShader' at line %d", Parser->CurrentToken.Line);
                return (NULL);
            }
            Parser->CurrentToken = NextToken(Parser);
            if (!ExpectToken(Parser, AX_TOKEN_STRING)) {
                SetParserError(Parser, "Expected fragment shader path string at line %d", Parser->CurrentToken.Line);
                return (NULL);
            }
            CopyTokenString(&Parser->CurrentToken, FragmentShaderPath, sizeof(FragmentShaderPath));
            Parser->CurrentToken = NextToken(Parser);
        } else {
            SetParserError(Parser, "Unknown material property '%.*s' at line %d",
                    (int)Parser->CurrentToken.Length, Parser->CurrentToken.Start, Parser->CurrentToken.Line);
            return (NULL);
        }

        // Skip optional newlines and comments
        while (Parser->CurrentToken.Type == AX_TOKEN_COMMENT ||
               Parser->CurrentToken.Type == AX_TOKEN_NEWLINE) {
            Parser->CurrentToken = NextToken(Parser);
        }
    }

    if (!ExpectToken(Parser, AX_TOKEN_RBRACE)) {
        SetParserError(Parser, "Expected '}' to close material block at line %d", Parser->CurrentToken.Line);
        return (NULL);
    }
    Parser->CurrentToken = NextToken(Parser);

    // Validate that both shader paths were provided
    if (VertexShaderPath[0] == '\0' || FragmentShaderPath[0] == '\0') {
        SetParserError(Parser, "Material '%s' must specify both vertexShader and fragmentShader", MaterialName);
        return (NULL);
    }

    // Create the material
    struct AxMaterial* Material = CreateMaterial(Scene, MaterialName, VertexShaderPath, FragmentShaderPath);
    if (!Material) {
        SetParserError(Parser, "Failed to create material '%s'", MaterialName);
        return (NULL);
    }

    // Trigger OnMaterialParsed callback
    InvokeCallbacks(GetMaterialCallback, Material);

    return (Material);
}

static AxLight* ParseLight(AxSceneParser* Parser, AxScene* Scene)
{
    if (!ExpectToken(Parser, AX_TOKEN_IDENTIFIER)) {
        SetParserError(Parser, "Expected 'light' keyword at line %d", Parser->CurrentToken.Line);
        return (NULL);
    }

    if (!TokenEquals(&Parser->CurrentToken, "light")) {
        SetParserError(Parser, "Expected 'light' keyword at line %d", Parser->CurrentToken.Line);
        return (NULL);
    }
    Parser->CurrentToken = NextToken(Parser);

    if (!ExpectToken(Parser, AX_TOKEN_STRING)) {
        SetParserError(Parser, "Expected light name string at line %d", Parser->CurrentToken.Line);
        return (NULL);
    }

    char LightName[64];
    CopyTokenString(&Parser->CurrentToken, LightName, sizeof(LightName));
    Parser->CurrentToken = NextToken(Parser);

    if (!ExpectToken(Parser, AX_TOKEN_LBRACE)) {
        SetParserError(Parser, "Expected '{' to start light block at line %d", Parser->CurrentToken.Line);
        return (NULL);
    }
    Parser->CurrentToken = NextToken(Parser);

    // Light properties with defaults
    AxLightType LightType = AX_LIGHT_TYPE_POINT;
    AxVec3 Position = { 0.0f, 0.0f, 0.0f };
    AxVec3 Direction = { 0.0f, -1.0f, 0.0f };
    AxVec3 Color = { 1.0f, 1.0f, 1.0f };
    float Intensity = 1.0f;
    float Range = 0.0f;
    float InnerCone = 0.5f;
    float OuterCone = 1.0f;

    while (Parser->CurrentToken.Type != AX_TOKEN_RBRACE && Parser->CurrentToken.Type != AX_TOKEN_EOF) {
        if (!ExpectToken(Parser, AX_TOKEN_IDENTIFIER)) {
            SetParserError(Parser, "Expected property name at line %d", Parser->CurrentToken.Line);
            return (NULL);
        }

        if (TokenEquals(&Parser->CurrentToken, "type")) {
            Parser->CurrentToken = NextToken(Parser);
            if (!ExpectToken(Parser, AX_TOKEN_COLON)) {
                SetParserError(Parser, "Expected ':' after 'type' at line %d", Parser->CurrentToken.Line);
                return (NULL);
            }
            Parser->CurrentToken = NextToken(Parser);
            if (!ExpectToken(Parser, AX_TOKEN_IDENTIFIER)) {
                SetParserError(Parser, "Expected light type identifier at line %d", Parser->CurrentToken.Line);
                return (NULL);
            }

            if (TokenEquals(&Parser->CurrentToken, "directional")) {
                LightType = AX_LIGHT_TYPE_DIRECTIONAL;
            } else if (TokenEquals(&Parser->CurrentToken, "point")) {
                LightType = AX_LIGHT_TYPE_POINT;
            } else if (TokenEquals(&Parser->CurrentToken, "spot")) {
                LightType = AX_LIGHT_TYPE_SPOT;
            } else {
                SetParserError(Parser, "Unknown light type '%.*s' at line %d",
                        (int)Parser->CurrentToken.Length, Parser->CurrentToken.Start, Parser->CurrentToken.Line);
                return (NULL);
            }
            Parser->CurrentToken = NextToken(Parser);
        } else if (TokenEquals(&Parser->CurrentToken, "position")) {
            Parser->CurrentToken = NextToken(Parser);
            if (!ExpectToken(Parser, AX_TOKEN_COLON)) {
                SetParserError(Parser, "Expected ':' after 'position' at line %d", Parser->CurrentToken.Line);
                return (NULL);
            }
            Parser->CurrentToken = NextToken(Parser);
            if (!ParseVector3(Parser, &Position)) {
                return (NULL);
            }
        } else if (TokenEquals(&Parser->CurrentToken, "direction")) {
            Parser->CurrentToken = NextToken(Parser);
            if (!ExpectToken(Parser, AX_TOKEN_COLON)) {
                SetParserError(Parser, "Expected ':' after 'direction' at line %d", Parser->CurrentToken.Line);
                return (NULL);
            }
            Parser->CurrentToken = NextToken(Parser);
            if (!ParseVector3(Parser, &Direction)) {
                return (NULL);
            }
        } else if (TokenEquals(&Parser->CurrentToken, "color")) {
            Parser->CurrentToken = NextToken(Parser);
            if (!ExpectToken(Parser, AX_TOKEN_COLON)) {
                SetParserError(Parser, "Expected ':' after 'color' at line %d", Parser->CurrentToken.Line);
                return (NULL);
            }
            Parser->CurrentToken = NextToken(Parser);
            if (!ParseVector3(Parser, &Color)) {
                return (NULL);
            }
        } else if (TokenEquals(&Parser->CurrentToken, "intensity")) {
            Parser->CurrentToken = NextToken(Parser);
            if (!ExpectToken(Parser, AX_TOKEN_COLON)) {
                SetParserError(Parser, "Expected ':' after 'intensity' at line %d", Parser->CurrentToken.Line);
                return (NULL);
            }
            Parser->CurrentToken = NextToken(Parser);
            if (!ExpectToken(Parser, AX_TOKEN_NUMBER)) {
                SetParserError(Parser, "Expected intensity number at line %d", Parser->CurrentToken.Line);
                return (NULL);
            }
            if (!ParseFloat(&Parser->CurrentToken, &Intensity)) {
                SetParserError(Parser, "Invalid intensity value at line %d", Parser->CurrentToken.Line);
                return (NULL);
            }
            Parser->CurrentToken = NextToken(Parser);
        } else if (TokenEquals(&Parser->CurrentToken, "range")) {
            Parser->CurrentToken = NextToken(Parser);
            if (!ExpectToken(Parser, AX_TOKEN_COLON)) {
                SetParserError(Parser, "Expected ':' after 'range' at line %d", Parser->CurrentToken.Line);
                return (NULL);
            }
            Parser->CurrentToken = NextToken(Parser);
            if (!ExpectToken(Parser, AX_TOKEN_NUMBER)) {
                SetParserError(Parser, "Expected range number at line %d", Parser->CurrentToken.Line);
                return (NULL);
            }
            if (!ParseFloat(&Parser->CurrentToken, &Range)) {
                SetParserError(Parser, "Invalid range value at line %d", Parser->CurrentToken.Line);
                return (NULL);
            }
            Parser->CurrentToken = NextToken(Parser);
        } else {
            SetParserError(Parser, "Unknown light property '%.*s' at line %d",
                    (int)Parser->CurrentToken.Length, Parser->CurrentToken.Start, Parser->CurrentToken.Line);
            return (NULL);
        }

        // Skip optional newlines and comments
        while (Parser->CurrentToken.Type == AX_TOKEN_COMMENT ||
               Parser->CurrentToken.Type == AX_TOKEN_NEWLINE) {
            Parser->CurrentToken = NextToken(Parser);
        }
    }

    if (!ExpectToken(Parser, AX_TOKEN_RBRACE)) {
        SetParserError(Parser, "Expected '}' to close light block at line %d", Parser->CurrentToken.Line);
        return (NULL);
    }
    Parser->CurrentToken = NextToken(Parser);

    // Create the light
    AxLight* Light = CreateLight(Scene, LightName, LightType);
    if (!Light) {
        SetParserError(Parser, "Failed to create light '%s'", LightName);
        return (NULL);
    }

    // Set light properties
    Light->Position = Position;
    Light->Direction = Direction;
    Light->Color = Color;
    Light->Intensity = Intensity;
    Light->Range = Range;
    Light->InnerConeAngle = InnerCone;
    Light->OuterConeAngle = OuterCone;

    // Trigger OnLightParsed callback
    InvokeCallbacks(GetLightCallback, Light);

    return (Light);
}

static AxScene* ParseScene(AxSceneParser* Parser)
{
    if (!ExpectToken(Parser, AX_TOKEN_IDENTIFIER)) {
        SetParserError(Parser, "Expected 'scene' keyword at line %d", Parser->CurrentToken.Line);
        return (NULL);
    }

    if (!TokenEquals(&Parser->CurrentToken, "scene")) {
        SetParserError(Parser, "Expected 'scene' keyword at line %d", Parser->CurrentToken.Line);
        return (NULL);
    }
    Parser->CurrentToken = NextToken(Parser);

    if (!ExpectToken(Parser, AX_TOKEN_STRING)) {
        SetParserError(Parser, "Expected scene name string at line %d", Parser->CurrentToken.Line);
        return (NULL);
    }

    char SceneName[64];
    CopyTokenString(&Parser->CurrentToken, SceneName, sizeof(SceneName));
    Parser->CurrentToken = NextToken(Parser);

    if (!ExpectToken(Parser, AX_TOKEN_LBRACE)) {
        SetParserError(Parser, "Expected '{' to start scene block at line %d", Parser->CurrentToken.Line);
        return (NULL);
    }
    Parser->CurrentToken = NextToken(Parser);

    // Create the scene using a reasonable default size
    AxScene* Scene = CreateScene(SceneName, Megabytes(1));
    if (!Scene) {
        SetParserError(Parser, "Failed to create scene '%s'", SceneName);
        return (NULL);
    }

    while (Parser->CurrentToken.Type != AX_TOKEN_RBRACE && Parser->CurrentToken.Type != AX_TOKEN_EOF) {
        if (Parser->CurrentToken.Type == AX_TOKEN_COMMENT ||
            Parser->CurrentToken.Type == AX_TOKEN_NEWLINE) {
            Parser->CurrentToken = NextToken(Parser);
            continue;
        }

        if (!ExpectToken(Parser, AX_TOKEN_IDENTIFIER)) {
            SetParserError(Parser, "Expected 'object' keyword at line %d", Parser->CurrentToken.Line);
            DestroyScene(Scene);
            return (NULL);
        }

        if (TokenEquals(&Parser->CurrentToken, "object")) {
            AxSceneObject* Object = ParseObject(Parser, Scene, NULL);
            if (!Object) {
                DestroyScene(Scene);
                return (NULL);
            }
        } else if (TokenEquals(&Parser->CurrentToken, "material")) {
            AxMaterial* Material = ParseMaterial(Parser, Scene);
            if (!Material) {
                DestroyScene(Scene);
                return (NULL);
            }
        } else if (TokenEquals(&Parser->CurrentToken, "light")) {
            AxLight* Light = ParseLight(Parser, Scene);
            if (!Light) {
                DestroyScene(Scene);
                return (NULL);
            }
        } else {
            SetParserError(Parser, "Unknown scene property '%.*s' at line %d",
                    (int)Parser->CurrentToken.Length, Parser->CurrentToken.Start, Parser->CurrentToken.Line);
            DestroyScene(Scene);
            return (NULL);
        }
    }

    if (!ExpectToken(Parser, AX_TOKEN_RBRACE)) {
        SetParserError(Parser, "Expected '}' to close scene block at line %d", Parser->CurrentToken.Line);
        DestroyScene(Scene);
        return (NULL);
    }

    return (Scene);
}

///////////////////////////////////////////////////////////////
// High-Level Loading and Parsing Functions
///////////////////////////////////////////////////////////////

static AxScene* ParseFromString(const char* Source, struct AxAllocator* Allocator)
{
    if (!Source || !Allocator) {
        strncpy(g_ParserLastErrorMessage, "Invalid parameters: Source and Allocator cannot be NULL",
                sizeof(g_ParserLastErrorMessage) - 1);
        return (NULL);
    }

    AxSceneParser Parser;
    InitParser(&Parser, Source, Allocator);

    // Skip initial comments and newlines
    while (Parser.CurrentToken.Type == AX_TOKEN_COMMENT ||
           Parser.CurrentToken.Type == AX_TOKEN_NEWLINE) {
        Parser.CurrentToken = NextToken(&Parser);
    }

    if (Parser.CurrentToken.Type == AX_TOKEN_EOF) {
        SetParserError(&Parser, "Empty scene file");
        return (NULL);
    }

    AxScene* Scene = ParseScene(&Parser);
    if (!Scene) {
        return (NULL);
    }

    return (Scene);
}

static AxScene* ParseFromFile(const char* FilePath, struct AxAllocator* Allocator)
{
    if (!FilePath || !Allocator) {
        strncpy(g_ParserLastErrorMessage, "Invalid parameters: FilePath and Allocator cannot be NULL",
                sizeof(g_ParserLastErrorMessage) - 1);
        return (NULL);
    }

    const struct AxPlatformFileAPI *FileAPI = PlatformAPI->FileAPI;
    AxFile File = FileAPI->OpenForRead(FilePath);
    if (!FileAPI->IsValid(File)) {
        snprintf(g_ParserLastErrorMessage, sizeof(g_ParserLastErrorMessage), "Failed to open file: %s", FilePath);
        return (NULL);
    }

    uint64_t FileSize = FileAPI->Size(File);
    if (FileSize <= 0) {
        FileAPI->Close(File);
        snprintf(g_ParserLastErrorMessage, sizeof(g_ParserLastErrorMessage),
                "File is empty or invalid: %s", FilePath);
        return (NULL);
    }

    char* FileContent = (char*)AxAlloc(Allocator, FileSize + 1);
    if (!FileContent) {
        FileAPI->Close(File);
        strncpy(g_ParserLastErrorMessage, "Failed to allocate memory for file content",
                sizeof(g_ParserLastErrorMessage) - 1);
        return (NULL);
    }

    uint64_t BytesRead = FileAPI->Read(File, FileContent, FileSize);
    FileAPI->Close(File);

    if (BytesRead != FileSize) {
        snprintf(g_ParserLastErrorMessage, sizeof(g_ParserLastErrorMessage),
        "Failed to read complete file: %s", FilePath);
        return (NULL);
    }

    FileContent[FileSize] = '\0';

    // Parse the content
    AxScene* Scene = ParseFromString(FileContent, Allocator);

    return (Scene);
}

static AxScene* LoadSceneFromFile(const char* FilePath)
{
    if (!FilePath) {
        SetError("Invalid file path: NULL");
        return (NULL);
    }

    // Create a PERSISTENT allocator for the Scene (NOT temporary!)
    // The Scene owns this allocator and it will be destroyed when Scene is destroyed
    struct AxAllocator* SceneAllocator = AllocatorAPI->CreateLinear("ScenePersistent", g_DefaultSceneMemorySize);
    if (!SceneAllocator) {
        SetError("Failed to create scene allocator");
        return (NULL);
    }

    // Parse the scene using the persistent allocator
    AxScene* Scene = ParseFromFile(FilePath, SceneAllocator);

    if (!Scene) {
        // If parsing failed, clean up the allocator
        SceneAllocator->Destroy(SceneAllocator);
        const char* ParseError = (g_ParserLastErrorMessage[0] != '\0' ? g_ParserLastErrorMessage : NULL);
        SetError("Failed to parse scene file '%s': %s", FilePath, ParseError ? ParseError : "Unknown error");
        return (NULL);
    }

    // Scene now owns the allocator - do NOT destroy it here!
    return (Scene);
}

static AxScene* LoadSceneFromString(const char* SceneData)
{
    if (!SceneData) {
        SetError("Invalid scene data: NULL");
        return (NULL);
    }

    // Create a PERSISTENT allocator for the Scene (NOT temporary!)
    // The Scene owns this allocator and it will be destroyed when Scene is destroyed
    struct AxAllocator* SceneAllocator = AllocatorAPI->CreateLinear("ScenePersistent", g_DefaultSceneMemorySize);
    if (!SceneAllocator) {
        SetError("Failed to create scene allocator");
        return (NULL);
    }

    // Parse the scene using the persistent allocator
    AxScene* Scene = ParseFromString(SceneData, SceneAllocator);

    if (!Scene) {
        // If parsing failed, clean up the allocator
        SceneAllocator->Destroy(SceneAllocator);
        const char* ParseError = (g_ParserLastErrorMessage[0] != '\0' ? g_ParserLastErrorMessage : NULL);
        SetError("Failed to parse scene data: %s", ParseError ? ParseError : "Unknown error");
        return (NULL);
    }

    // Scene now owns the allocator - do NOT destroy it here!
    return (Scene);
}

///////////////////////////////////////////////////////////////
// Configuration and Utility Functions
///////////////////////////////////////////////////////////////

static const char* GetLastError(void)
{
    return (g_LastErrorMessage[0] != '\0' ? g_LastErrorMessage : NULL);
}

static void SetDefaultSceneMemorySize(size_t MemorySize)
{
    if (MemorySize > 0) {
        g_DefaultSceneMemorySize = MemorySize;
    }
}

static size_t GetDefaultSceneMemorySize(void)
{
    return (g_DefaultSceneMemorySize);
}

static const char* GetParserLastError(void)
{
    return (g_ParserLastErrorMessage[0] != '\0' ? g_ParserLastErrorMessage : NULL);
}

static void FreeScene(AxScene* Scene)
{
    if (Scene) {
        DestroyScene(Scene);
    }
}

///////////////////////////////////////////////////////////////
// API Structure and Plugin Interface
///////////////////////////////////////////////////////////////

struct AxSceneAPI* SceneAPI = &(struct AxSceneAPI) {
    .CreateScene = CreateScene,
    .DestroyScene = DestroyScene,
    .CreateObject = CreateObject,
    .SetObjectTransform = SetObjectTransform,
    .SetObjectMesh = SetObjectMesh,
    .SetObjectMaterial = SetObjectMaterial,
    .CreateMaterial = CreateMaterial,
    .FindMaterial = FindMaterial,
    .FindObject = FindObject,
    .CreateLight = CreateLight,
    .FindLight = FindLight,
    .AddCamera = AddCamera,
    .GetCamera = GetCamera,
    .GetWorldTransform = GetWorldTransform,
    .LoadSceneFromFile = LoadSceneFromFile,
    .LoadSceneFromString = LoadSceneFromString,
    .ParseFromString = ParseFromString,
    .ParseFromFile = ParseFromFile,
    .SetDefaultSceneMemorySize = SetDefaultSceneMemorySize,
    .GetDefaultSceneMemorySize = GetDefaultSceneMemorySize,
    .GetLastError = GetLastError,
    .RegisterEventHandler = RegisterEventHandler,
    .UnregisterEventHandler = UnregisterEventHandler,
    .ClearEventHandlers = ClearEventHandlers
};

// Plugin load function - exported for dynamic loading
AXON_DLL_EXPORT void LoadPlugin(struct AxAPIRegistry* APIRegistry, bool Load)
{
    if (APIRegistry) {
        AllocatorAPI = (struct AxAllocatorAPI *)APIRegistry->Get(AXON_ALLOCATOR_API_NAME);
        PlatformAPI = (struct AxPlatformAPI *)APIRegistry->Get(AXON_PLATFORM_API_NAME);

        // Register the Scene API with the API registry
        APIRegistry->Set(AXON_SCENE_API_NAME, SceneAPI, sizeof(struct AxSceneAPI));
    }
}
