#include "Foundation/AxAllocatorAPI.h"
#include "Foundation/AxPlugin.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxMath.h"
#include "Foundation/AxPlatform.h"
#include "Foundation/AxHashTable.h"
#include "AxScene/AxScene.h"
#include "AxEngine/AxSceneTree.h"
#include "AxEngine/AxNode.h"
#include "AxEngine/AxTypedNodes.h"
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

static void InvokeOnSceneParsed(const SceneTree* Scene)
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
// Type Name -> NodeType Mapping
///////////////////////////////////////////////////////////////

static NodeType MapTypeNameToNodeType(const char* TypeName)
{
    if (strcmp(TypeName, "MeshInstance") == 0)     { return (NodeType::MeshInstance); }
    if (strcmp(TypeName, "Camera") == 0)           { return (NodeType::Camera); }
    if (strcmp(TypeName, "Light") == 0)            { return (NodeType::Light); }
    if (strcmp(TypeName, "RigidBody") == 0)        { return (NodeType::RigidBody); }
    if (strcmp(TypeName, "Collider") == 0)         { return (NodeType::Collider); }
    if (strcmp(TypeName, "AudioSource") == 0)      { return (NodeType::AudioSource); }
    if (strcmp(TypeName, "AudioListener") == 0)    { return (NodeType::AudioListener); }
    if (strcmp(TypeName, "Animator") == 0)         { return (NodeType::Animator); }
    if (strcmp(TypeName, "ParticleEmitter") == 0)  { return (NodeType::ParticleEmitter); }
    if (strcmp(TypeName, "Sprite") == 0)           { return (NodeType::Sprite); }
    if (strcmp(TypeName, "Node3D") == 0)           { return (NodeType::Node3D); }
    if (strcmp(TypeName, "Node2D") == 0)           { return (NodeType::Node2D); }
    return (NodeType::Base); // indicates unknown
}

///////////////////////////////////////////////////////////////
// Typed Node Property Setting
///////////////////////////////////////////////////////////////

static void SetNodeProperty(Node* NodePtr, NodeType Type,
                             const char* PropName, const char* ValueStr,
                             float NumericValue, bool IsString, bool IsNumeric,
                             AxSceneParser* Parser)
{
    switch (Type) {
        case NodeType::MeshInstance: {
            MeshInstance* MI = static_cast<MeshInstance*>(NodePtr);
            if (strcmp(PropName, "mesh") == 0 && IsString) {
                MI->SetMeshPath(ValueStr);
            } else if (strcmp(PropName, "material") == 0 && IsString) {
                MI->SetMaterialName(ValueStr);
            } else if (strcmp(PropName, "renderLayer") == 0 && IsNumeric) {
                MI->RenderLayer = static_cast<int32_t>(NumericValue);
            }
            break;
        }
        case NodeType::Camera: {
            CameraNode* Cam = static_cast<CameraNode*>(NodePtr);
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
            break;
        }
        case NodeType::Light: {
            LightNode* LN = static_cast<LightNode*>(NodePtr);
            if (strcmp(PropName, "type") == 0 && !IsString && !IsNumeric) {
                if (strcmp(ValueStr, "directional") == 0)      { LN->SetLightType(AX_LIGHT_TYPE_DIRECTIONAL); }
                else if (strcmp(ValueStr, "point") == 0)       { LN->SetLightType(AX_LIGHT_TYPE_POINT); }
                else if (strcmp(ValueStr, "spot") == 0)        { LN->SetLightType(AX_LIGHT_TYPE_SPOT); }
            } else if (strcmp(PropName, "intensity") == 0 && IsNumeric) {
                LN->SetIntensity(NumericValue);
            } else if (strcmp(PropName, "range") == 0 && IsNumeric) {
                LN->Light.Range = NumericValue;
            } else if (strcmp(PropName, "innerCone") == 0 && IsNumeric) {
                LN->Light.InnerConeAngle = NumericValue;
            } else if (strcmp(PropName, "outerCone") == 0 && IsNumeric) {
                LN->Light.OuterConeAngle = NumericValue;
            } else if (strcmp(PropName, "direction") == 0) {
                // direction is parsed as vector3 inline; handled at parse time
            } else if (strcmp(PropName, "color") == 0) {
                // color is parsed as vector3 inline; handled at parse time
            } else if (strcmp(PropName, "position") == 0) {
                // position is parsed as vector3 inline; handled at parse time
            }
            break;
        }
        case NodeType::RigidBody: {
            RigidBodyNode* RB = static_cast<RigidBodyNode*>(NodePtr);
            if (strcmp(PropName, "mass") == 0 && IsNumeric) {
                RB->Mass = NumericValue;
            } else if (strcmp(PropName, "drag") == 0 && IsNumeric) {
                RB->Drag = NumericValue;
            } else if (strcmp(PropName, "bodyType") == 0 && !IsString && !IsNumeric) {
                if (strcmp(ValueStr, "dynamic") == 0)        { RB->Type = BodyType::Dynamic; }
                else if (strcmp(ValueStr, "static") == 0)    { RB->Type = BodyType::Static; }
                else if (strcmp(ValueStr, "kinematic") == 0)  { RB->Type = BodyType::Kinematic; }
            }
            break;
        }
        case NodeType::AudioSource: {
            AudioSourceNode* AS = static_cast<AudioSourceNode*>(NodePtr);
            if (strcmp(PropName, "clip") == 0 && IsString) {
                strncpy(AS->ClipPath, ValueStr, sizeof(AS->ClipPath) - 1);
                AS->ClipPath[sizeof(AS->ClipPath) - 1] = '\0';
            } else if (strcmp(PropName, "volume") == 0 && IsNumeric) {
                AS->Volume = NumericValue;
            } else if (strcmp(PropName, "pitch") == 0 && IsNumeric) {
                AS->Pitch = NumericValue;
            }
            break;
        }
        case NodeType::Collider: {
            ColliderNode* Col = static_cast<ColliderNode*>(NodePtr);
            if (strcmp(PropName, "shape") == 0 && !IsString && !IsNumeric) {
                if (strcmp(ValueStr, "box") == 0)         { Col->Shape = ShapeType::Box; }
                else if (strcmp(ValueStr, "sphere") == 0) { Col->Shape = ShapeType::Sphere; }
                else if (strcmp(ValueStr, "capsule") == 0){ Col->Shape = ShapeType::Capsule; }
                else if (strcmp(ValueStr, "mesh") == 0)   { Col->Shape = ShapeType::Mesh; }
            } else if (strcmp(PropName, "radius") == 0 && IsNumeric) {
                Col->Radius = NumericValue;
            } else if (strcmp(PropName, "height") == 0 && IsNumeric) {
                Col->Height = NumericValue;
            } else if (strcmp(PropName, "isTrigger") == 0 && !IsString && !IsNumeric) {
                Col->IsTrigger = (strcmp(ValueStr, "true") == 0);
            }
            break;
        }
        case NodeType::Animator: {
            AnimatorNode* Anim = static_cast<AnimatorNode*>(NodePtr);
            if (strcmp(PropName, "animation") == 0 && IsString) {
                strncpy(Anim->AnimationName, ValueStr, sizeof(Anim->AnimationName) - 1);
                Anim->AnimationName[sizeof(Anim->AnimationName) - 1] = '\0';
            } else if (strcmp(PropName, "speed") == 0 && IsNumeric) {
                Anim->Speed = NumericValue;
            }
            break;
        }
        case NodeType::ParticleEmitter: {
            ParticleEmitterNode* PE = static_cast<ParticleEmitterNode*>(NodePtr);
            if (strcmp(PropName, "maxParticles") == 0 && IsNumeric) {
                PE->MaxParticles = static_cast<uint32_t>(NumericValue);
            } else if (strcmp(PropName, "emissionRate") == 0 && IsNumeric) {
                PE->EmissionRate = NumericValue;
            } else if (strcmp(PropName, "lifetime") == 0 && IsNumeric) {
                PE->Lifetime = NumericValue;
            } else if (strcmp(PropName, "speed") == 0 && IsNumeric) {
                PE->Speed = NumericValue;
            }
            break;
        }
        case NodeType::Sprite: {
            SpriteNode* Spr = static_cast<SpriteNode*>(NodePtr);
            if (strcmp(PropName, "texture") == 0 && IsString) {
                strncpy(Spr->TexturePath, ValueStr, sizeof(Spr->TexturePath) - 1);
                Spr->TexturePath[sizeof(Spr->TexturePath) - 1] = '\0';
            } else if (strcmp(PropName, "sortOrder") == 0 && IsNumeric) {
                Spr->SortOrder = static_cast<int32_t>(NumericValue);
            }
            break;
        }
        default:
            break;
    }
}

///////////////////////////////////////////////////////////////
// Node Parsing
///////////////////////////////////////////////////////////////

// Forward declaration for recursive nodes.
static Node* ParseNode(AxSceneParser* Parser, SceneTree* Scene, Node* Parent);

static Node* ParseNode(AxSceneParser* Parser, SceneTree* Scene, Node* Parent)
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

    // Check for optional type name identifier before '{'
    NodeType Type = NodeType::Node3D; // default if no type specified
    while (Parser->CurrentToken.Type == AX_TOKEN_COMMENT ||
           Parser->CurrentToken.Type == AX_TOKEN_NEWLINE) {
        Parser->CurrentToken = NextToken(Parser);
    }

    if (Parser->CurrentToken.Type == AX_TOKEN_IDENTIFIER) {
        // This could be a type name or the opening brace follows
        char TypeName[64];
        CopyTokenString(&Parser->CurrentToken, TypeName, sizeof(TypeName));

        NodeType MappedType = MapTypeNameToNodeType(TypeName);
        if (MappedType != NodeType::Base) {
            // Valid type name
            Type = MappedType;
            Parser->CurrentToken = NextToken(Parser);
        } else {
            // Unknown type name -- error
            SetParserError(Parser, "Unknown node type '%s' at line %d",
                           TypeName, Parser->CurrentToken.Line);
            return (nullptr);
        }
    }

    if (!ExpectToken(Parser, AX_TOKEN_LBRACE)) {
        SetParserError(Parser, "Expected '{' to start node block at line %d", Parser->CurrentToken.Line);
        return (nullptr);
    }
    Parser->CurrentToken = NextToken(Parser);

    Node* NewNode = Scene->CreateNode(NodeName, Type, Parent);
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
        } else if (TokenEquals(&Parser->CurrentToken, "node")) {
            if (!ParseNode(Parser, Scene, NewNode)) {
                return (nullptr);
            }
        } else {
            // Inline property: key: value
            char PropName[64];
            CopyTokenString(&Parser->CurrentToken, PropName, sizeof(PropName));
            Parser->CurrentToken = NextToken(Parser);

            if (!ExpectToken(Parser, AX_TOKEN_COLON)) {
                SetParserError(Parser, "Expected ':' after property name '%s' at line %d",
                               PropName, Parser->CurrentToken.Line);
                return (nullptr);
            }
            Parser->CurrentToken = NextToken(Parser);

            // Handle vector3 properties for Light nodes
            if (Type == NodeType::Light) {
                if (strcmp(PropName, "direction") == 0) {
                    LightNode* LN = static_cast<LightNode*>(NewNode);
                    if (!ParseVector3(Parser, &LN->Light.Direction)) { return (nullptr); }
                    continue;
                } else if (strcmp(PropName, "color") == 0) {
                    LightNode* LN = static_cast<LightNode*>(NewNode);
                    if (!ParseVector3(Parser, &LN->Light.Color)) { return (nullptr); }
                    continue;
                } else if (strcmp(PropName, "position") == 0) {
                    LightNode* LN = static_cast<LightNode*>(NewNode);
                    if (!ParseVector3(Parser, &LN->Light.Position)) { return (nullptr); }
                    continue;
                }
            }

            char ValueStr[256] = {0};
            float NumericValue = 0.0f;
            bool IsString = false;
            bool IsNumeric = false;

            // Skip whitespace tokens before the value
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

            SetNodeProperty(NewNode, Type, PropName, ValueStr, NumericValue, IsString, IsNumeric, Parser);
        }
    }

    if (!ExpectToken(Parser, AX_TOKEN_RBRACE)) {
        SetParserError(Parser, "Expected '}' to close node block at line %d", Parser->CurrentToken.Line);
        return (nullptr);
    }
    Parser->CurrentToken = NextToken(Parser);

    // For Light nodes, invoke OnLightParsed with the AxLight data
    if (Type == NodeType::Light) {
        LightNode* LN = static_cast<LightNode*>(NewNode);
        InvokeOnLightParsed(&LN->Light);
    }

    InvokeOnNodeParsed(NewNode);
    return (NewNode);
}

///////////////////////////////////////////////////////////////
// Scene Parsing
///////////////////////////////////////////////////////////////

static SceneTree* ParseScene(AxSceneParser* Parser)
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

    SceneTree* Scene = new SceneTree(g_HashTableAPI, Parser->Allocator);
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

        if (TokenEquals(&Parser->CurrentToken, "node")) {
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

static SceneTree* ParseFromString(const char* Source, struct AxAllocator* Allocator)
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

static SceneTree* ParseFromFile(const char* FilePath, struct AxAllocator* Allocator)
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

static SceneTree* LoadSceneFromFile(const char* FilePath)
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

    SceneTree* Scene = ParseFromFile(FilePath, SceneAllocator);
    if (!Scene) {
        SceneAllocator->Destroy(SceneAllocator);
        const char* ParseError = g_ParserLastErrorMessage[0] != '\0' ? g_ParserLastErrorMessage : nullptr;
        SetError("Failed to parse scene file '%s': %s", FilePath, ParseError ? ParseError : "Unknown error");
        return (nullptr);
    }

    return (Scene);
}

static SceneTree* LoadSceneFromString(const char* SceneData)
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

    SceneTree* Scene = ParseFromString(SceneData, SceneAllocator);
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

Node* ParsePrefab(const char* PrefabData, SceneTree* Scene)
{
    if (!PrefabData || !Scene) {
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

// Deep-copies a node subtree into Scene, creating new independent nodes.
static Node* DeepCopyNode(SceneTree* Scene, Node* Src, Node* NewParent)
{
    if (!Src || !Scene) {
        return (nullptr);
    }

    Node* Copy = Scene->CreateNode(Src->GetName(), Src->GetType(), NewParent);
    if (!Copy) {
        return (nullptr);
    }

    Copy->GetTransform() = Src->GetTransform();

    for (Node* Child = Src->GetFirstChild(); Child; Child = Child->GetNextSibling()) {
        DeepCopyNode(Scene, Child, Copy);
    }

    return (Copy);
}

Node* InstantiatePrefab(SceneTree* Scene, Node* PrefabRoot,
                         Node* Parent, struct AxAllocator* Allocator)
{
    if (!Scene || !PrefabRoot) {
        return (nullptr);
    }

    (void)Allocator; // No longer needed; typed nodes are allocated by SceneTree

    return (DeepCopyNode(Scene, PrefabRoot, Parent));
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

    g_AllocatorAPI = static_cast<AxAllocatorAPI*>(Registry->Get(AXON_ALLOCATOR_API_NAME));
    g_PlatformAPI  = static_cast<AxPlatformAPI*>(Registry->Get(AXON_PLATFORM_API_NAME));
    g_HashTableAPI = static_cast<AxHashTableAPI*>(Registry->Get(AXON_HASH_TABLE_API_NAME));
}

void AxSceneParser_Term(void)
{
    g_AllocatorAPI   = nullptr;
    g_PlatformAPI    = nullptr;
    g_HashTableAPI   = nullptr;
}

struct AxSceneParserAPI* AxSceneParser_GetAPI(void)
{
    return (SceneParserAPI);
}
