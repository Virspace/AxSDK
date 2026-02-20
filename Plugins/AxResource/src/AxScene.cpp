#include "Foundation/AxAllocatorAPI.h"
#include "Foundation/AxPlugin.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxMath.h"
#include "Foundation/AxPlatform.h"
#include "Foundation/AxHashTable.h"
#include "AxScene/AxScene.h"
#include "AxEngine/AxScene.h"
#include "AxEngine/AxNode.h"
#include "AxEngine/AxComponent.h"
#include "AxEngine/AxComponentFactory.h"
#include "AxStandardComponents.h"
#include "AxSceneParserInternal.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cctype>

struct AxSceneParser {
    const char* Source;
    size_t SourceLength;
    size_t CurrentPos;
    int CurrentLine;
    int CurrentColumn;
    AxToken CurrentToken;
    char ErrorMessage[256];
    struct AxAllocator* Allocator;
};

///////////////////////////////////////////////////////////////
// Global State
///////////////////////////////////////////////////////////////

static size_t g_DefaultSceneMemorySize = 1024 * 1024;
static char g_LastErrorMessage[256] = {0};
static char g_ParserLastErrorMessage[256] = {0};

#define MAX_EVENT_HANDLERS 16
static AxSceneEvents g_EventHandlers[MAX_EVENT_HANDLERS];
static int g_EventHandlerCount = 0;

static AxAllocatorAPI* g_AllocatorAPI = nullptr;
static AxPlatformAPI*  g_PlatformAPI  = nullptr;
static AxHashTableAPI* g_HashTableAPI = nullptr;
static ComponentFactory* gParserFactory = nullptr;

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

    for (int i = 0; i < g_EventHandlerCount; i++) {
        if (memcmp(&g_EventHandlers[i], Events, sizeof(AxSceneEvents)) == 0) {
            SetError("Event handler already registered");
            return (AX_SCENE_ERROR_ALREADY_EXISTS);
        }
    }

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

    for (int i = 0; i < g_EventHandlerCount; i++) {
        if (memcmp(&g_EventHandlers[i], Events, sizeof(AxSceneEvents)) == 0) {
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

static void InvokeOnLightParsed(const AxLight* Light)
{
    for (int i = 0; i < g_EventHandlerCount; ++i) {
        if (g_EventHandlers[i].OnLightParsed) {
            g_EventHandlers[i].OnLightParsed(Light, g_EventHandlers[i].UserData);
        }
    }
}

static void InvokeOnSceneParsed(const AxScene* Scene)
{
    for (int i = 0; i < g_EventHandlerCount; ++i) {
        if (g_EventHandlers[i].OnSceneParsed) {
            g_EventHandlers[i].OnSceneParsed(Scene, g_EventHandlers[i].UserData);
        }
    }
}

static void InvokeOnNodeParsed(Node* ParsedNode)
{
    for (int i = 0; i < g_EventHandlerCount; ++i) {
        if (g_EventHandlers[i].OnNodeParsed) {
            g_EventHandlers[i].OnNodeParsed(
                static_cast<void*>(ParsedNode), g_EventHandlers[i].UserData);
        }
    }
}

static void InvokeOnComponentParsed(Component* Comp, Node* Owner)
{
    for (int i = 0; i < g_EventHandlerCount; ++i) {
        if (g_EventHandlers[i].OnComponentParsed) {
            g_EventHandlers[i].OnComponentParsed(
                static_cast<void*>(Comp),
                static_cast<void*>(Owner),
                g_EventHandlers[i].UserData);
        }
    }
}

///////////////////////////////////////////////////////////////
// Tokenizer
///////////////////////////////////////////////////////////////

static AxToken NextToken(AxSceneParser* Parser);

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
    while (!IsAtEnd(Parser) && CurrentChar(Parser) != '\n') {
        AdvanceChar(Parser);
    }
}

static AxToken ReadString(AxSceneParser* Parser)
{
    AxToken Token = {};
    Token.Type = AX_TOKEN_STRING;
    Token.Line = Parser->CurrentLine;
    Token.Column = Parser->CurrentColumn;
    Token.Start = &Parser->Source[Parser->CurrentPos];

    AdvanceChar(Parser); // skip opening quote

    const char* StringStart = &Parser->Source[Parser->CurrentPos];

    while (!IsAtEnd(Parser) && CurrentChar(Parser) != '"') {
        if (CurrentChar(Parser) == '\n') {
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
    Token.Start = StringStart;

    AdvanceChar(Parser); // skip closing quote
    return (Token);
}

static AxToken ReadNumber(AxSceneParser* Parser)
{
    AxToken Token = {};
    Token.Type = AX_TOKEN_NUMBER;
    Token.Line = Parser->CurrentLine;
    Token.Column = Parser->CurrentColumn;
    Token.Start = &Parser->Source[Parser->CurrentPos];

    if (CurrentChar(Parser) == '-' || CurrentChar(Parser) == '+') {
        AdvanceChar(Parser);
    }

    while (!IsAtEnd(Parser) && isdigit(CurrentChar(Parser))) {
        AdvanceChar(Parser);
    }

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
    AxToken Token = {};
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

static AxToken NextToken(AxSceneParser* Parser)
{
    SkipWhitespace(Parser);

    AxToken Token = {};
    Token.Line = Parser->CurrentLine;
    Token.Column = Parser->CurrentColumn;
    Token.Start = &Parser->Source[Parser->CurrentPos];

    if (IsAtEnd(Parser)) {
        Token.Type = AX_TOKEN_EOF;
        Token.Length = 0;
        return (Token);
    }

    char C = CurrentChar(Parser);

    if (C == '#') {
        Token.Type = AX_TOKEN_COMMENT;
        SkipComment(Parser);
        Token.Length = &Parser->Source[Parser->CurrentPos] - Token.Start;
        return (Token);
    }

    if (C == '\n') {
        Token.Type = AX_TOKEN_NEWLINE;
        Token.Length = 1;
        AdvanceChar(Parser);
        return (Token);
    }

    switch (C) {
        case '{': Token.Type = AX_TOKEN_LBRACE; Token.Length = 1; AdvanceChar(Parser); return (Token);
        case '}': Token.Type = AX_TOKEN_RBRACE; Token.Length = 1; AdvanceChar(Parser); return (Token);
        case ':': Token.Type = AX_TOKEN_COLON;  Token.Length = 1; AdvanceChar(Parser); return (Token);
        case ',': Token.Type = AX_TOKEN_COMMA;  Token.Length = 1; AdvanceChar(Parser); return (Token);
    }

    if (C == '"') {
        return (ReadString(Parser));
    }

    if (isdigit(C) || C == '-' || C == '+') {
        return (ReadNumber(Parser));
    }

    if (isalpha(C) || C == '_') {
        return (ReadIdentifier(Parser));
    }

    Token.Type = AX_TOKEN_INVALID;
    Token.Length = 1;
    AdvanceChar(Parser);
    return (Token);
}

static bool TokenEquals(AxToken* Token, const char* Expected)
{
    size_t ExpectedLen = strlen(Expected);
    return (Token->Length == ExpectedLen &&
            strncmp(Token->Start, Expected, Token->Length) == 0);
}

// Skips over COMMENT and NEWLINE tokens, then checks the type.
static bool ExpectToken(AxSceneParser* Parser, AxTokenType Type)
{
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
    size_t CopyLen = Token->Length < sizeof(Buffer) - 1 ? Token->Length : sizeof(Buffer) - 1;
    strncpy(Buffer, Token->Start, CopyLen);
    Buffer[CopyLen] = '\0';

    char* EndPtr;
    *Result = strtof(Buffer, &EndPtr);
    return (EndPtr != Buffer);
}

static void CopyTokenString(AxToken* Token, char* Dest, size_t DestSize)
{
    size_t CopyLen = Token->Length < DestSize - 1 ? Token->Length : DestSize - 1;
    strncpy(Dest, Token->Start, CopyLen);
    Dest[CopyLen] = '\0';
}

///////////////////////////////////////////////////////////////
// Shared Sub-Parsers
///////////////////////////////////////////////////////////////

static bool ParseVector3(AxSceneParser* Parser, AxVec3* Vector)
{
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
    if (!ExpectToken(Parser, AX_TOKEN_NUMBER)) {
        SetParserError(Parser, "Expected number for quaternion X at line %d", Parser->CurrentToken.Line);
        return (false);
    }
    if (!ParseFloat(&Parser->CurrentToken, &Quaternion->X)) { return (false); }
    Parser->CurrentToken = NextToken(Parser);

    if (!ExpectToken(Parser, AX_TOKEN_COMMA)) { return (false); }
    Parser->CurrentToken = NextToken(Parser);

    if (!ExpectToken(Parser, AX_TOKEN_NUMBER)) { return (false); }
    if (!ParseFloat(&Parser->CurrentToken, &Quaternion->Y)) { return (false); }
    Parser->CurrentToken = NextToken(Parser);

    if (!ExpectToken(Parser, AX_TOKEN_COMMA)) { return (false); }
    Parser->CurrentToken = NextToken(Parser);

    if (!ExpectToken(Parser, AX_TOKEN_NUMBER)) { return (false); }
    if (!ParseFloat(&Parser->CurrentToken, &Quaternion->Z)) { return (false); }
    Parser->CurrentToken = NextToken(Parser);

    if (!ExpectToken(Parser, AX_TOKEN_COMMA)) { return (false); }
    Parser->CurrentToken = NextToken(Parser);

    if (!ExpectToken(Parser, AX_TOKEN_NUMBER)) { return (false); }
    if (!ParseFloat(&Parser->CurrentToken, &Quaternion->W)) { return (false); }
    Parser->CurrentToken = NextToken(Parser);

    return (true);
}

// Parses a "transform { ... }" block.
// Called with Parser->CurrentToken == '{' (caller already advanced past "transform").
static bool ParseTransform(AxSceneParser* Parser, AxTransform* Transform)
{
    Transform->Translation = AxVec3{0.0f, 0.0f, 0.0f};
    Transform->Rotation    = AxQuat{0.0f, 0.0f, 0.0f, 1.0f};
    Transform->Scale       = AxVec3{1.0f, 1.0f, 1.0f};
    Transform->Up          = AxVec3{0.0f, 1.0f, 0.0f};
    Transform->Parent      = nullptr;
    Transform->LastComputedFrame   = 0;
    Transform->ForwardMatrixDirty  = true;
    Transform->InverseMatrixDirty  = true;
    Transform->IsIdentity          = false;

    if (!ExpectToken(Parser, AX_TOKEN_LBRACE)) {
        SetParserError(Parser, "Expected '{' to start transform block at line %d", Parser->CurrentToken.Line);
        return (false);
    }
    Parser->CurrentToken = NextToken(Parser);

    while (Parser->CurrentToken.Type != AX_TOKEN_RBRACE &&
           Parser->CurrentToken.Type != AX_TOKEN_EOF) {
        if (!ExpectToken(Parser, AX_TOKEN_IDENTIFIER)) {
            SetParserError(Parser, "Expected transform property at line %d", Parser->CurrentToken.Line);
            return (false);
        }

        if (TokenEquals(&Parser->CurrentToken, "translation")) {
            Parser->CurrentToken = NextToken(Parser);
            if (!ExpectToken(Parser, AX_TOKEN_COLON)) { return (false); }
            Parser->CurrentToken = NextToken(Parser);
            if (!ParseVector3(Parser, &Transform->Translation)) { return (false); }
        } else if (TokenEquals(&Parser->CurrentToken, "rotation")) {
            Parser->CurrentToken = NextToken(Parser);
            if (!ExpectToken(Parser, AX_TOKEN_COLON)) { return (false); }
            Parser->CurrentToken = NextToken(Parser);
            if (!ParseQuaternion(Parser, &Transform->Rotation)) { return (false); }
        } else if (TokenEquals(&Parser->CurrentToken, "scale")) {
            Parser->CurrentToken = NextToken(Parser);
            if (!ExpectToken(Parser, AX_TOKEN_COLON)) { return (false); }
            Parser->CurrentToken = NextToken(Parser);
            if (!ParseVector3(Parser, &Transform->Scale)) { return (false); }
        } else {
            SetParserError(Parser, "Unknown transform property at line %d", Parser->CurrentToken.Line);
            return (false);
        }

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

///////////////////////////////////////////////////////////////
// Component Parsing
///////////////////////////////////////////////////////////////

static void SetComponentProperty(Component* Comp, const char* TypeName,
                                  const char* PropName, const char* ValueStr,
                                  float NumericValue, bool IsString, bool IsNumeric)
{
    if (strcmp(TypeName, "MeshFilter") == 0) {
        MeshFilter* MF = static_cast<MeshFilter*>(Comp);
        if (strcmp(PropName, "mesh") == 0 && IsString) {
            MF->SetMeshPath(ValueStr);
        }
        return;
    }

    if (strcmp(TypeName, "MeshRenderer") == 0) {
        MeshRenderer* MR = static_cast<MeshRenderer*>(Comp);
        if (strcmp(PropName, "material") == 0 && IsString) {
            MR->SetMaterialName(ValueStr);
        } else if (strcmp(PropName, "renderLayer") == 0 && IsNumeric) {
            MR->RenderLayer = static_cast<int32_t>(NumericValue);
        }
        return;
    }

    if (strcmp(TypeName, "Camera") == 0 || strcmp(TypeName, "CameraComponent") == 0) {
        CameraComponent* Cam = static_cast<CameraComponent*>(Comp);
        if (strcmp(PropName, "fov") == 0 && IsNumeric) {
            Cam->SetFOV(NumericValue);
        } else if (strcmp(PropName, "near") == 0 && IsNumeric) {
            Cam->SetNear(NumericValue);
        } else if (strcmp(PropName, "far") == 0 && IsNumeric) {
            Cam->SetFar(NumericValue);
        } else if (strcmp(PropName, "projection") == 0 && !IsString && !IsNumeric) {
            if (strcmp(ValueStr, "perspective") == 0) {
                Cam->SetOrthographic(false);
            } else if (strcmp(ValueStr, "orthographic") == 0) {
                Cam->SetOrthographic(true);
            }
        }
        return;
    }

    if (strcmp(TypeName, "LightComponent") == 0) {
        LightComponent* Lit = static_cast<LightComponent*>(Comp);
        if (strcmp(PropName, "intensity") == 0 && IsNumeric) {
            Lit->SetIntensity(NumericValue);
        }
        return;
    }

    if (strcmp(TypeName, "RigidBody") == 0) {
        RigidBody* RB = static_cast<RigidBody*>(Comp);
        if (strcmp(PropName, "mass") == 0 && IsNumeric) {
            RB->Mass = NumericValue;
        } else if (strcmp(PropName, "drag") == 0 && IsNumeric) {
            RB->Drag = NumericValue;
        } else if (strcmp(PropName, "bodyType") == 0 && !IsString && !IsNumeric) {
            if (strcmp(ValueStr, "dynamic") == 0)        { RB->Type = BodyType::Dynamic; }
            else if (strcmp(ValueStr, "static") == 0)   { RB->Type = BodyType::Static; }
            else if (strcmp(ValueStr, "kinematic") == 0) { RB->Type = BodyType::Kinematic; }
        }
        return;
    }

    if (strcmp(TypeName, "AudioSource") == 0) {
        AudioSource* AS = static_cast<AudioSource*>(Comp);
        if (strcmp(PropName, "clip") == 0 && IsString) {
            strncpy(AS->ClipPath, ValueStr, sizeof(AS->ClipPath) - 1);
            AS->ClipPath[sizeof(AS->ClipPath) - 1] = '\0';
        } else if (strcmp(PropName, "volume") == 0 && IsNumeric) {
            AS->Volume = NumericValue;
        }
        return;
    }
}

// Parses a "component TypeName { ... }" block and attaches it to Owner.
// Called with Parser->CurrentToken == "component".
static Component* ParseComponent(AxSceneParser* Parser, AxScene* Scene, Node* Owner)
{
    Parser->CurrentToken = NextToken(Parser); // advance past "component"

    if (!ExpectToken(Parser, AX_TOKEN_IDENTIFIER)) {
        SetParserError(Parser, "Expected component type name at line %d", Parser->CurrentToken.Line);
        return (nullptr);
    }

    char TypeName[64];
    CopyTokenString(&Parser->CurrentToken, TypeName, sizeof(TypeName));
    Parser->CurrentToken = NextToken(Parser);

    // "Camera" is registered under "CameraComponent" in the factory
    const char* FactoryTypeName = (strcmp(TypeName, "Camera") == 0) ? "CameraComponent" : TypeName;

    if (!gParserFactory) {
        SetParserError(Parser, "ComponentFactory not initialized");
        return (nullptr);
    }

    Component* Comp = gParserFactory->CreateComponent(FactoryTypeName, Scene->Allocator);
    if (!Comp) {
        SetParserError(Parser, "Unknown component type '%s' at line %d",
                       TypeName, Parser->CurrentToken.Line);
        return (nullptr);
    }

    if (!ExpectToken(Parser, AX_TOKEN_LBRACE)) {
        SetParserError(Parser, "Expected '{' after component type at line %d", Parser->CurrentToken.Line);
        gParserFactory->DestroyComponent(Comp, Scene->Allocator);
        return (nullptr);
    }
    Parser->CurrentToken = NextToken(Parser);

    while (Parser->CurrentToken.Type != AX_TOKEN_RBRACE &&
           Parser->CurrentToken.Type != AX_TOKEN_EOF) {
        if (Parser->CurrentToken.Type == AX_TOKEN_COMMENT ||
            Parser->CurrentToken.Type == AX_TOKEN_NEWLINE) {
            Parser->CurrentToken = NextToken(Parser);
            continue;
        }

        if (!ExpectToken(Parser, AX_TOKEN_IDENTIFIER)) {
            break;
        }

        char PropName[64];
        CopyTokenString(&Parser->CurrentToken, PropName, sizeof(PropName));
        Parser->CurrentToken = NextToken(Parser);

        if (!ExpectToken(Parser, AX_TOKEN_COLON)) {
            break;
        }
        Parser->CurrentToken = NextToken(Parser);

        char ValueStr[256] = {0};
        float NumericValue = 0.0f;
        bool IsString = false;
        bool IsNumeric = false;

        // Skip any whitespace tokens before the value
        while (Parser->CurrentToken.Type == AX_TOKEN_COMMENT ||
               Parser->CurrentToken.Type == AX_TOKEN_NEWLINE) {
            Parser->CurrentToken = NextToken(Parser);
        }

        if (Parser->CurrentToken.Type == AX_TOKEN_STRING) {
            CopyTokenString(&Parser->CurrentToken, ValueStr, sizeof(ValueStr));
            IsString = true;
            Parser->CurrentToken = NextToken(Parser);
        } else if (Parser->CurrentToken.Type == AX_TOKEN_NUMBER) {
            ParseFloat(&Parser->CurrentToken, &NumericValue);
            IsNumeric = true;
            Parser->CurrentToken = NextToken(Parser);
        } else if (Parser->CurrentToken.Type == AX_TOKEN_IDENTIFIER) {
            CopyTokenString(&Parser->CurrentToken, ValueStr, sizeof(ValueStr));
            Parser->CurrentToken = NextToken(Parser);
        }

        SetComponentProperty(Comp, TypeName, PropName, ValueStr, NumericValue, IsString, IsNumeric);
    }

    if (!ExpectToken(Parser, AX_TOKEN_RBRACE)) {
        SetParserError(Parser, "Expected '}' to close component block at line %d", Parser->CurrentToken.Line);
        gParserFactory->DestroyComponent(Comp, Scene->Allocator);
        return (nullptr);
    }
    Parser->CurrentToken = NextToken(Parser);

    if (!Scene->AddComponent(Owner, Comp)) {
        SetParserError(Parser, "Failed to add component '%s' to node", TypeName);
        gParserFactory->DestroyComponent(Comp, Scene->Allocator);
        return (nullptr);
    }

    InvokeOnComponentParsed(Comp, Owner);
    return (Comp);
}

///////////////////////////////////////////////////////////////
// Node Parsing
///////////////////////////////////////////////////////////////

// Forward declaration for recursive nodes.
static Node* ParseNode(AxSceneParser* Parser, AxScene* Scene, Node* Parent);

static Node* ParseNode(AxSceneParser* Parser, AxScene* Scene, Node* Parent)
{
    // Called with Parser->CurrentToken == "node" identifier.
    Parser->CurrentToken = NextToken(Parser); // advance past "node"

    if (!ExpectToken(Parser, AX_TOKEN_STRING)) {
        SetParserError(Parser, "Expected node name string at line %d", Parser->CurrentToken.Line);
        return (nullptr);
    }

    char NodeName[64];
    CopyTokenString(&Parser->CurrentToken, NodeName, sizeof(NodeName));
    Parser->CurrentToken = NextToken(Parser);

    if (!ExpectToken(Parser, AX_TOKEN_LBRACE)) {
        SetParserError(Parser, "Expected '{' to start node block at line %d", Parser->CurrentToken.Line);
        return (nullptr);
    }
    Parser->CurrentToken = NextToken(Parser);

    Node* NewNode = Scene->CreateNode(NodeName, NodeType::Base, Parent);
    if (!NewNode) {
        SetParserError(Parser, "Failed to create node '%s'", NodeName);
        return (nullptr);
    }

    while (Parser->CurrentToken.Type != AX_TOKEN_RBRACE &&
           Parser->CurrentToken.Type != AX_TOKEN_EOF) {
        if (Parser->CurrentToken.Type == AX_TOKEN_COMMENT ||
            Parser->CurrentToken.Type == AX_TOKEN_NEWLINE) {
            Parser->CurrentToken = NextToken(Parser);
            continue;
        }

        if (!ExpectToken(Parser, AX_TOKEN_IDENTIFIER)) {
            SetParserError(Parser, "Expected keyword in node block at line %d", Parser->CurrentToken.Line);
            return (nullptr);
        }

        if (TokenEquals(&Parser->CurrentToken, "transform")) {
            Parser->CurrentToken = NextToken(Parser); // advance past "transform"
            AxTransform Transform;
            if (!ParseTransform(Parser, &Transform)) {
                return (nullptr);
            }
            NewNode->GetTransform() = Transform;
        } else if (TokenEquals(&Parser->CurrentToken, "component")) {
            if (!ParseComponent(Parser, Scene, NewNode)) {
                return (nullptr);
            }
        } else if (TokenEquals(&Parser->CurrentToken, "node")) {
            if (!ParseNode(Parser, Scene, NewNode)) {
                return (nullptr);
            }
        } else {
            SetParserError(Parser, "Unknown keyword '%.*s' in node block at line %d",
                           static_cast<int>(Parser->CurrentToken.Length),
                           Parser->CurrentToken.Start,
                           Parser->CurrentToken.Line);
            return (nullptr);
        }
    }

    if (!ExpectToken(Parser, AX_TOKEN_RBRACE)) {
        SetParserError(Parser, "Expected '}' to close node block at line %d", Parser->CurrentToken.Line);
        return (nullptr);
    }
    Parser->CurrentToken = NextToken(Parser);

    InvokeOnNodeParsed(NewNode);
    return (NewNode);
}

///////////////////////////////////////////////////////////////
// Light Parsing
///////////////////////////////////////////////////////////////

static AxLight* ParseLight(AxSceneParser* Parser, AxScene* Scene)
{
    if (!ExpectToken(Parser, AX_TOKEN_IDENTIFIER) ||
        !TokenEquals(&Parser->CurrentToken, "light")) {
        SetParserError(Parser, "Expected 'light' keyword at line %d", Parser->CurrentToken.Line);
        return (nullptr);
    }
    Parser->CurrentToken = NextToken(Parser);

    if (!ExpectToken(Parser, AX_TOKEN_STRING)) {
        SetParserError(Parser, "Expected light name string at line %d", Parser->CurrentToken.Line);
        return (nullptr);
    }

    char LightName[64];
    CopyTokenString(&Parser->CurrentToken, LightName, sizeof(LightName));
    Parser->CurrentToken = NextToken(Parser);

    if (!ExpectToken(Parser, AX_TOKEN_LBRACE)) {
        SetParserError(Parser, "Expected '{' to start light block at line %d", Parser->CurrentToken.Line);
        return (nullptr);
    }
    Parser->CurrentToken = NextToken(Parser);

    AxLightType LightType = AX_LIGHT_TYPE_POINT;
    AxVec3 Position    = {0.0f,  0.0f, 0.0f};
    AxVec3 Direction   = {0.0f, -1.0f, 0.0f};
    AxVec3 Color       = {1.0f,  1.0f, 1.0f};
    float Intensity    = 1.0f;
    float Range        = 0.0f;
    float InnerCone    = 0.5f;
    float OuterCone    = 1.0f;

    while (Parser->CurrentToken.Type != AX_TOKEN_RBRACE &&
           Parser->CurrentToken.Type != AX_TOKEN_EOF) {
        if (!ExpectToken(Parser, AX_TOKEN_IDENTIFIER)) {
            SetParserError(Parser, "Expected property name at line %d", Parser->CurrentToken.Line);
            return (nullptr);
        }

        if (TokenEquals(&Parser->CurrentToken, "type")) {
            Parser->CurrentToken = NextToken(Parser);
            if (!ExpectToken(Parser, AX_TOKEN_COLON)) { return (nullptr); }
            Parser->CurrentToken = NextToken(Parser);
            if (!ExpectToken(Parser, AX_TOKEN_IDENTIFIER)) { return (nullptr); }

            if (TokenEquals(&Parser->CurrentToken, "directional"))      { LightType = AX_LIGHT_TYPE_DIRECTIONAL; }
            else if (TokenEquals(&Parser->CurrentToken, "point"))       { LightType = AX_LIGHT_TYPE_POINT; }
            else if (TokenEquals(&Parser->CurrentToken, "spot"))        { LightType = AX_LIGHT_TYPE_SPOT; }
            else {
                SetParserError(Parser, "Unknown light type '%.*s' at line %d",
                               static_cast<int>(Parser->CurrentToken.Length),
                               Parser->CurrentToken.Start, Parser->CurrentToken.Line);
                return (nullptr);
            }
            Parser->CurrentToken = NextToken(Parser);
        } else if (TokenEquals(&Parser->CurrentToken, "position")) {
            Parser->CurrentToken = NextToken(Parser);
            if (!ExpectToken(Parser, AX_TOKEN_COLON)) { return (nullptr); }
            Parser->CurrentToken = NextToken(Parser);
            if (!ParseVector3(Parser, &Position)) { return (nullptr); }
        } else if (TokenEquals(&Parser->CurrentToken, "direction")) {
            Parser->CurrentToken = NextToken(Parser);
            if (!ExpectToken(Parser, AX_TOKEN_COLON)) { return (nullptr); }
            Parser->CurrentToken = NextToken(Parser);
            if (!ParseVector3(Parser, &Direction)) { return (nullptr); }
        } else if (TokenEquals(&Parser->CurrentToken, "color")) {
            Parser->CurrentToken = NextToken(Parser);
            if (!ExpectToken(Parser, AX_TOKEN_COLON)) { return (nullptr); }
            Parser->CurrentToken = NextToken(Parser);
            if (!ParseVector3(Parser, &Color)) { return (nullptr); }
        } else if (TokenEquals(&Parser->CurrentToken, "intensity")) {
            Parser->CurrentToken = NextToken(Parser);
            if (!ExpectToken(Parser, AX_TOKEN_COLON)) { return (nullptr); }
            Parser->CurrentToken = NextToken(Parser);
            if (!ExpectToken(Parser, AX_TOKEN_NUMBER)) { return (nullptr); }
            if (!ParseFloat(&Parser->CurrentToken, &Intensity)) { return (nullptr); }
            Parser->CurrentToken = NextToken(Parser);
        } else if (TokenEquals(&Parser->CurrentToken, "range")) {
            Parser->CurrentToken = NextToken(Parser);
            if (!ExpectToken(Parser, AX_TOKEN_COLON)) { return (nullptr); }
            Parser->CurrentToken = NextToken(Parser);
            if (!ExpectToken(Parser, AX_TOKEN_NUMBER)) { return (nullptr); }
            if (!ParseFloat(&Parser->CurrentToken, &Range)) { return (nullptr); }
            Parser->CurrentToken = NextToken(Parser);
        } else {
            SetParserError(Parser, "Unknown light property '%.*s' at line %d",
                           static_cast<int>(Parser->CurrentToken.Length),
                           Parser->CurrentToken.Start, Parser->CurrentToken.Line);
            return (nullptr);
        }

        while (Parser->CurrentToken.Type == AX_TOKEN_COMMENT ||
               Parser->CurrentToken.Type == AX_TOKEN_NEWLINE) {
            Parser->CurrentToken = NextToken(Parser);
        }
    }

    if (!ExpectToken(Parser, AX_TOKEN_RBRACE)) {
        SetParserError(Parser, "Expected '}' to close light block at line %d", Parser->CurrentToken.Line);
        return (nullptr);
    }
    Parser->CurrentToken = NextToken(Parser);

    AxLight* Light = Scene->CreateLight(LightName, LightType);
    if (!Light) {
        SetParserError(Parser, "Failed to create light '%s'", LightName);
        return (nullptr);
    }

    Light->Position       = Position;
    Light->Direction      = Direction;
    Light->Color          = Color;
    Light->Intensity      = Intensity;
    Light->Range          = Range;
    Light->InnerConeAngle = InnerCone;
    Light->OuterConeAngle = OuterCone;

    InvokeOnLightParsed(Light);
    return (Light);
}

///////////////////////////////////////////////////////////////
// Scene Parsing
///////////////////////////////////////////////////////////////

static AxScene* ParseScene(AxSceneParser* Parser)
{
    if (!ExpectToken(Parser, AX_TOKEN_IDENTIFIER) ||
        !TokenEquals(&Parser->CurrentToken, "scene")) {
        SetParserError(Parser, "Expected 'scene' keyword at line %d", Parser->CurrentToken.Line);
        return (nullptr);
    }
    Parser->CurrentToken = NextToken(Parser);

    if (!ExpectToken(Parser, AX_TOKEN_STRING)) {
        SetParserError(Parser, "Expected scene name string at line %d", Parser->CurrentToken.Line);
        return (nullptr);
    }

    char SceneName[64];
    CopyTokenString(&Parser->CurrentToken, SceneName, sizeof(SceneName));
    Parser->CurrentToken = NextToken(Parser);

    if (!ExpectToken(Parser, AX_TOKEN_LBRACE)) {
        SetParserError(Parser, "Expected '{' to start scene block at line %d", Parser->CurrentToken.Line);
        return (nullptr);
    }
    Parser->CurrentToken = NextToken(Parser);

    AxScene* Scene = new AxScene(g_HashTableAPI, Parser->Allocator);
    Scene->Name = SceneName;

    while (Parser->CurrentToken.Type != AX_TOKEN_RBRACE &&
           Parser->CurrentToken.Type != AX_TOKEN_EOF) {
        if (Parser->CurrentToken.Type == AX_TOKEN_COMMENT ||
            Parser->CurrentToken.Type == AX_TOKEN_NEWLINE) {
            Parser->CurrentToken = NextToken(Parser);
            continue;
        }

        if (!ExpectToken(Parser, AX_TOKEN_IDENTIFIER)) {
            SetParserError(Parser, "Expected keyword at line %d", Parser->CurrentToken.Line);
            delete Scene;
            return (nullptr);
        }

        if (TokenEquals(&Parser->CurrentToken, "light")) {
            if (!ParseLight(Parser, Scene)) {
                delete Scene;
                return (nullptr);
            }
        } else if (TokenEquals(&Parser->CurrentToken, "node")) {
            if (!ParseNode(Parser, Scene, nullptr)) {
                delete Scene;
                return (nullptr);
            }
        } else {
            SetParserError(Parser, "Unknown scene keyword '%.*s' at line %d",
                           static_cast<int>(Parser->CurrentToken.Length),
                           Parser->CurrentToken.Start, Parser->CurrentToken.Line);
            delete Scene;
            return (nullptr);
        }
    }

    if (!ExpectToken(Parser, AX_TOKEN_RBRACE)) {
        SetParserError(Parser, "Expected '}' to close scene block at line %d", Parser->CurrentToken.Line);
        delete Scene;
        return (nullptr);
    }

    InvokeOnSceneParsed(Scene);
    return (Scene);
}

///////////////////////////////////////////////////////////////
// High-Level Loading Functions
///////////////////////////////////////////////////////////////

static AxScene* ParseFromString(const char* Source, struct AxAllocator* Allocator)
{
    if (!Source || !Allocator) {
        strncpy(g_ParserLastErrorMessage, "Invalid parameters: Source and Allocator cannot be NULL",
                sizeof(g_ParserLastErrorMessage) - 1);
        return (nullptr);
    }

    AxSceneParser Parser;
    InitParser(&Parser, Source, Allocator);

    while (Parser.CurrentToken.Type == AX_TOKEN_COMMENT ||
           Parser.CurrentToken.Type == AX_TOKEN_NEWLINE) {
        Parser.CurrentToken = NextToken(&Parser);
    }

    if (Parser.CurrentToken.Type == AX_TOKEN_EOF) {
        SetParserError(&Parser, "Empty scene file");
        return (nullptr);
    }

    return (ParseScene(&Parser));
}

static AxScene* ParseFromFile(const char* FilePath, struct AxAllocator* Allocator)
{
    if (!FilePath || !Allocator) {
        strncpy(g_ParserLastErrorMessage, "Invalid parameters: FilePath and Allocator cannot be NULL",
                sizeof(g_ParserLastErrorMessage) - 1);
        return (nullptr);
    }

    const AxPlatformFileAPI* FileAPI = g_PlatformAPI->FileAPI;
    AxFile File = FileAPI->OpenForRead(FilePath);
    if (!FileAPI->IsValid(File)) {
        snprintf(g_ParserLastErrorMessage, sizeof(g_ParserLastErrorMessage),
                 "Failed to open file: %s", FilePath);
        return (nullptr);
    }

    uint64_t FileSize = FileAPI->Size(File);
    if (FileSize == 0) {
        FileAPI->Close(File);
        snprintf(g_ParserLastErrorMessage, sizeof(g_ParserLastErrorMessage),
                 "File is empty: %s", FilePath);
        return (nullptr);
    }

    char* FileContent = static_cast<char*>(AxAlloc(Allocator, FileSize + 1));
    if (!FileContent) {
        FileAPI->Close(File);
        strncpy(g_ParserLastErrorMessage, "Failed to allocate memory for file content",
                sizeof(g_ParserLastErrorMessage) - 1);
        return (nullptr);
    }

    uint64_t BytesRead = FileAPI->Read(File, FileContent, FileSize);
    FileAPI->Close(File);

    if (BytesRead != FileSize) {
        snprintf(g_ParserLastErrorMessage, sizeof(g_ParserLastErrorMessage),
                 "Failed to read complete file: %s", FilePath);
        return (nullptr);
    }

    FileContent[FileSize] = '\0';
    return (ParseFromString(FileContent, Allocator));
}

static AxScene* LoadSceneFromFile(const char* FilePath)
{
    if (!FilePath) {
        SetError("Invalid file path: NULL");
        return (nullptr);
    }

    AxAllocator* SceneAllocator = g_AllocatorAPI->CreateLinear("ScenePersistent", g_DefaultSceneMemorySize);
    if (!SceneAllocator) {
        SetError("Failed to create scene allocator");
        return (nullptr);
    }

    AxScene* Scene = ParseFromFile(FilePath, SceneAllocator);
    if (!Scene) {
        SceneAllocator->Destroy(SceneAllocator);
        const char* ParseError = g_ParserLastErrorMessage[0] != '\0' ? g_ParserLastErrorMessage : nullptr;
        SetError("Failed to parse scene file '%s': %s", FilePath, ParseError ? ParseError : "Unknown error");
        return (nullptr);
    }

    return (Scene);
}

static AxScene* LoadSceneFromString(const char* SceneData)
{
    if (!SceneData) {
        SetError("Invalid scene data: NULL");
        return (nullptr);
    }

    AxAllocator* SceneAllocator = g_AllocatorAPI->CreateLinear("ScenePersistent", g_DefaultSceneMemorySize);
    if (!SceneAllocator) {
        SetError("Failed to create scene allocator");
        return (nullptr);
    }

    AxScene* Scene = ParseFromString(SceneData, SceneAllocator);
    if (!Scene) {
        SceneAllocator->Destroy(SceneAllocator);
        const char* ParseError = g_ParserLastErrorMessage[0] != '\0' ? g_ParserLastErrorMessage : nullptr;
        SetError("Failed to parse scene data: %s", ParseError ? ParseError : "Unknown error");
        return (nullptr);
    }

    return (Scene);
}

///////////////////////////////////////////////////////////////
// Prefab Loading and Instantiation
///////////////////////////////////////////////////////////////

// Parses a .axp prefab string into a node subtree attached to Scene.
// Prefab format: a single top-level "node" block with no scene wrapper.
Node* ParsePrefab(const char* PrefabData, AxScene* Scene)
{
    if (!PrefabData || !Scene || !gParserFactory) {
        return (nullptr);
    }

    AxAllocator* Allocator = Scene->Allocator;
    if (!Allocator && g_AllocatorAPI) {
        Allocator = g_AllocatorAPI->CreateHeap("PrefabAlloc", 64 * 1024, 256 * 1024);
    }
    if (!Allocator) {
        return (nullptr);
    }

    AxSceneParser Parser;
    InitParser(&Parser, PrefabData, Allocator);

    while (Parser.CurrentToken.Type == AX_TOKEN_COMMENT ||
           Parser.CurrentToken.Type == AX_TOKEN_NEWLINE) {
        Parser.CurrentToken = NextToken(&Parser);
    }

    if (!ExpectToken(&Parser, AX_TOKEN_IDENTIFIER) ||
        !TokenEquals(&Parser.CurrentToken, "node")) {
        return (nullptr);
    }

    return (ParseNode(&Parser, Scene, nullptr));
}

// Deep-copies a node subtree into Scene, assigning new NodeIDs and
// re-registering all components in the scene's global lists.
static Node* DeepCopyNode(AxScene* Scene, Node* Src, Node* NewParent,
                           ComponentFactory* Factory, AxAllocator* Allocator)
{
    if (!Src || !Scene) {
        return (nullptr);
    }

    Node* Copy = Scene->CreateNode(Src->GetName(), Src->GetType(), NewParent);
    if (!Copy) {
        return (nullptr);
    }

    Copy->GetTransform() = Src->GetTransform();

    if (Factory) {
        uint32_t TypeCount = Factory->GetTypeCount();
        for (uint32_t i = 0; i < TypeCount; ++i) {
            const ComponentTypeInfo* Info = Factory->GetTypeAtIndex(i);
            if (!Info) { continue; }

            Component* SrcComp = Src->GetComponent(Info->TypeID);
            if (!SrcComp) { continue; }

            Component* NewComp = Factory->CreateComponent(Info->TypeName, Allocator);
            if (!NewComp) { continue; }

            // Copy concrete component data past the base Component fields.
            size_t BaseSize  = sizeof(Component);
            size_t TotalSize = Info->DataSize;
            if (TotalSize > BaseSize) {
                memcpy(reinterpret_cast<char*>(NewComp) + BaseSize,
                       reinterpret_cast<const char*>(SrcComp) + BaseSize,
                       TotalSize - BaseSize);
            }

            Scene->AddComponent(Copy, NewComp);
        }
    }

    for (Node* Child = Src->GetFirstChild(); Child; Child = Child->GetNextSibling()) {
        DeepCopyNode(Scene, Child, Copy, Factory, Allocator);
    }

    return (Copy);
}

Node* InstantiatePrefab(AxScene* Scene, Node* PrefabRoot,
                         Node* Parent, AxAllocator* Allocator)
{
    if (!Scene || !PrefabRoot) {
        return (nullptr);
    }

    AxAllocator* Alloc = Allocator ? Allocator : Scene->Allocator;
    if (!Alloc && g_AllocatorAPI) {
        Alloc = g_AllocatorAPI->CreateHeap("PrefabInstAlloc", 64 * 1024, 256 * 1024);
    }

    return (DeepCopyNode(Scene, PrefabRoot, Parent, gParserFactory, Alloc));
}

///////////////////////////////////////////////////////////////
// Utility
///////////////////////////////////////////////////////////////

static const char* GetLastError(void)
{
    return (g_LastErrorMessage[0] != '\0' ? g_LastErrorMessage : nullptr);
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

///////////////////////////////////////////////////////////////
// API Structure
///////////////////////////////////////////////////////////////

extern void RegisterCoreSystems(void);
extern void UnregisterCoreSystems(void);

static AxSceneParserAPI SceneParserAPIStruct = {
    .ParseFromString          = ParseFromString,
    .ParseFromFile            = ParseFromFile,
    .LoadSceneFromFile        = LoadSceneFromFile,
    .LoadSceneFromString      = LoadSceneFromString,
    .SetDefaultSceneMemorySize = SetDefaultSceneMemorySize,
    .GetDefaultSceneMemorySize = GetDefaultSceneMemorySize,
    .GetLastError             = GetLastError,
    .RegisterEventHandler     = RegisterEventHandler,
    .UnregisterEventHandler   = UnregisterEventHandler,
    .ClearEventHandlers       = ClearEventHandlers,
};

static AxSceneParserAPI* SceneParserAPI = &SceneParserAPIStruct;

///////////////////////////////////////////////////////////////
// Initialization (called by AxResource)
///////////////////////////////////////////////////////////////

void AxSceneParser_Init(struct AxAPIRegistry* Registry)
{
    if (!Registry) {
        return;
    }

    fprintf(stderr, "[PARSER] Getting APIs...\n");
    g_AllocatorAPI = static_cast<AxAllocatorAPI*>(Registry->Get(AXON_ALLOCATOR_API_NAME));
    g_PlatformAPI  = static_cast<AxPlatformAPI*>(Registry->Get(AXON_PLATFORM_API_NAME));
    g_HashTableAPI = static_cast<AxHashTableAPI*>(Registry->Get(AXON_HASH_TABLE_API_NAME));
    fprintf(stderr, "[PARSER] AllocatorAPI=%p PlatformAPI=%p HashTableAPI=%p\n",
            (void*)g_AllocatorAPI, (void*)g_PlatformAPI, (void*)g_HashTableAPI);

    fprintf(stderr, "[PARSER] Getting ComponentFactory...\n");
    gParserFactory = AxComponentFactory_Get();
    fprintf(stderr, "[PARSER] ComponentFactory=%p\n", (void*)gParserFactory);

    if (gParserFactory) {
        fprintf(stderr, "[PARSER] Registering standard components...\n");
        RegisterStandardComponents(gParserFactory);
        fprintf(stderr, "[PARSER] Standard components registered\n");
    }

    fprintf(stderr, "[PARSER] Registering core systems...\n");
    RegisterCoreSystems();
    fprintf(stderr, "[PARSER] Core systems registered\n");
}

void AxSceneParser_Term(void)
{
    UnregisterCoreSystems();
    gParserFactory = nullptr;
    g_AllocatorAPI   = nullptr;
    g_PlatformAPI    = nullptr;
    g_HashTableAPI   = nullptr;
}

struct AxSceneParserAPI* AxSceneParser_GetAPI(void)
{
    return (SceneParserAPI);
}
