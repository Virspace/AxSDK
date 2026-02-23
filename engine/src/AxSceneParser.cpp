#include "Foundation/AxAllocatorAPI.h"
#include "Foundation/AxPlugin.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxMath.h"
#include "Foundation/AxPlatform.h"
#include "Foundation/AxHashTable.h"
#include "AxEngine/AxSceneParser.h"
#include "AxEngine/AxSceneTree.h"
#include "AxEngine/AxNode.h"
#include "AxEngine/AxTypedNodes.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cctype>

///////////////////////////////////////////////////////////////
// Tokenizer (internal parsing state)
///////////////////////////////////////////////////////////////

struct Tokenizer {
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
// Tokenizer Helpers (file-local, no class state needed)
///////////////////////////////////////////////////////////////

static AxToken NextToken(Tokenizer* T);

static void InitTokenizer(Tokenizer* T, const char* Source, struct AxAllocator* Allocator)
{
    T->Source = Source;
    T->SourceLength = strlen(Source);
    T->CurrentPos = 0;
    T->CurrentLine = 1;
    T->CurrentColumn = 1;
    T->ErrorMessage[0] = '\0';
    T->Allocator = Allocator;
    T->CurrentToken = NextToken(T);
}

static bool IsAtEnd(Tokenizer* T)
{
    return (T->CurrentPos >= T->SourceLength);
}

static char CurrentChar(Tokenizer* T)
{
    if (IsAtEnd(T)) {
        return ('\0');
    }
    return (T->Source[T->CurrentPos]);
}

static char PeekChar(Tokenizer* T, int Offset)
{
    size_t Pos = T->CurrentPos + Offset;
    if (Pos >= T->SourceLength) {
        return ('\0');
    }
    return (T->Source[Pos]);
}

static void AdvanceChar(Tokenizer* T)
{
    if (!IsAtEnd(T)) {
        if (CurrentChar(T) == '\n') {
            T->CurrentLine++;
            T->CurrentColumn = 1;
        } else {
            T->CurrentColumn++;
        }
        T->CurrentPos++;
    }
}

static void SkipWhitespace(Tokenizer* T)
{
    while (!IsAtEnd(T)) {
        char C = CurrentChar(T);
        if (C == ' ' || C == '\t' || C == '\r') {
            AdvanceChar(T);
        } else {
            break;
        }
    }
}

static void SkipComment(Tokenizer* T)
{
    while (!IsAtEnd(T) && CurrentChar(T) != '\n') {
        AdvanceChar(T);
    }
}

static AxToken ReadString(Tokenizer* T)
{
    AxToken Token = {};
    Token.Type = AX_TOKEN_STRING;
    Token.Line = T->CurrentLine;
    Token.Column = T->CurrentColumn;
    Token.Start = &T->Source[T->CurrentPos];

    AdvanceChar(T); // skip opening quote

    const char* StringStart = &T->Source[T->CurrentPos];

    while (!IsAtEnd(T) && CurrentChar(T) != '"') {
        if (CurrentChar(T) == '\n') {
            Token.Type = AX_TOKEN_INVALID;
            return (Token);
        }
        AdvanceChar(T);
    }

    if (IsAtEnd(T)) {
        Token.Type = AX_TOKEN_INVALID;
        return (Token);
    }

    Token.Length = &T->Source[T->CurrentPos] - StringStart;
    Token.Start = StringStart;

    AdvanceChar(T); // skip closing quote
    return (Token);
}

static AxToken ReadNumber(Tokenizer* T)
{
    AxToken Token = {};
    Token.Type = AX_TOKEN_NUMBER;
    Token.Line = T->CurrentLine;
    Token.Column = T->CurrentColumn;
    Token.Start = &T->Source[T->CurrentPos];

    if (CurrentChar(T) == '-' || CurrentChar(T) == '+') {
        AdvanceChar(T);
    }

    while (!IsAtEnd(T) && isdigit(CurrentChar(T))) {
        AdvanceChar(T);
    }

    if (!IsAtEnd(T) && CurrentChar(T) == '.') {
        AdvanceChar(T);
        while (!IsAtEnd(T) && isdigit(CurrentChar(T))) {
            AdvanceChar(T);
        }
    }

    Token.Length = &T->Source[T->CurrentPos] - Token.Start;
    return (Token);
}

static AxToken ReadIdentifier(Tokenizer* T)
{
    AxToken Token = {};
    Token.Type = AX_TOKEN_IDENTIFIER;
    Token.Line = T->CurrentLine;
    Token.Column = T->CurrentColumn;
    Token.Start = &T->Source[T->CurrentPos];

    while (!IsAtEnd(T)) {
        char C = CurrentChar(T);
        if (isalnum(C) || C == '_') {
            AdvanceChar(T);
        } else {
            break;
        }
    }

    Token.Length = &T->Source[T->CurrentPos] - Token.Start;
    return (Token);
}

static AxToken NextToken(Tokenizer* T)
{
    SkipWhitespace(T);

    AxToken Token = {};
    Token.Line = T->CurrentLine;
    Token.Column = T->CurrentColumn;
    Token.Start = &T->Source[T->CurrentPos];

    if (IsAtEnd(T)) {
        Token.Type = AX_TOKEN_EOF;
        Token.Length = 0;
        return (Token);
    }

    char C = CurrentChar(T);

    if (C == '#') {
        Token.Type = AX_TOKEN_COMMENT;
        SkipComment(T);
        Token.Length = &T->Source[T->CurrentPos] - Token.Start;
        return (Token);
    }

    if (C == '\n') {
        Token.Type = AX_TOKEN_NEWLINE;
        Token.Length = 1;
        AdvanceChar(T);
        return (Token);
    }

    switch (C) {
        case '{': Token.Type = AX_TOKEN_LBRACE; Token.Length = 1; AdvanceChar(T); return (Token);
        case '}': Token.Type = AX_TOKEN_RBRACE; Token.Length = 1; AdvanceChar(T); return (Token);
        case ':': Token.Type = AX_TOKEN_COLON;  Token.Length = 1; AdvanceChar(T); return (Token);
        case ',': Token.Type = AX_TOKEN_COMMA;  Token.Length = 1; AdvanceChar(T); return (Token);
    }

    if (C == '"') {
        return (ReadString(T));
    }

    if (isdigit(C) || C == '-' || C == '+') {
        return (ReadNumber(T));
    }

    if (isalpha(C) || C == '_') {
        return (ReadIdentifier(T));
    }

    Token.Type = AX_TOKEN_INVALID;
    Token.Length = 1;
    AdvanceChar(T);
    return (Token);
}

static bool TokenEquals(AxToken* Token, const char* Expected)
{
    size_t ExpectedLen = strlen(Expected);
    return (Token->Length == ExpectedLen &&
            strncmp(Token->Start, Expected, Token->Length) == 0);
}

// Skips over COMMENT and NEWLINE tokens, then checks the type.
static bool ExpectToken(Tokenizer* T, AxTokenType Type)
{
    while (T->CurrentToken.Type == AX_TOKEN_COMMENT ||
           T->CurrentToken.Type == AX_TOKEN_NEWLINE) {
        T->CurrentToken = NextToken(T);
    }
    return (T->CurrentToken.Type == Type);
}

static bool ParseFloatToken(AxToken* Token, float* Result)
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
// Shared Sub-Parsers (file-local, use SetParserError via class)
///////////////////////////////////////////////////////////////

static bool ParseVector3(Tokenizer* T, AxVec3* Vector, SceneParser* SP)
{
    if (!ExpectToken(T, AX_TOKEN_NUMBER)) {
        SP->SetParserError(T, "Expected number for X component at line %d", T->CurrentToken.Line);
        return (false);
    }
    if (!ParseFloatToken(&T->CurrentToken, &Vector->X)) {
        SP->SetParserError(T, "Invalid number format for X component at line %d", T->CurrentToken.Line);
        return (false);
    }
    T->CurrentToken = NextToken(T);

    if (!ExpectToken(T, AX_TOKEN_COMMA)) {
        SP->SetParserError(T, "Expected ',' after X component at line %d", T->CurrentToken.Line);
        return (false);
    }
    T->CurrentToken = NextToken(T);

    if (!ExpectToken(T, AX_TOKEN_NUMBER)) {
        SP->SetParserError(T, "Expected number for Y component at line %d", T->CurrentToken.Line);
        return (false);
    }
    if (!ParseFloatToken(&T->CurrentToken, &Vector->Y)) {
        SP->SetParserError(T, "Invalid number format for Y component at line %d", T->CurrentToken.Line);
        return (false);
    }
    T->CurrentToken = NextToken(T);

    if (!ExpectToken(T, AX_TOKEN_COMMA)) {
        SP->SetParserError(T, "Expected ',' after Y component at line %d", T->CurrentToken.Line);
        return (false);
    }
    T->CurrentToken = NextToken(T);

    if (!ExpectToken(T, AX_TOKEN_NUMBER)) {
        SP->SetParserError(T, "Expected number for Z component at line %d", T->CurrentToken.Line);
        return (false);
    }
    if (!ParseFloatToken(&T->CurrentToken, &Vector->Z)) {
        SP->SetParserError(T, "Invalid number format for Z component at line %d", T->CurrentToken.Line);
        return (false);
    }
    T->CurrentToken = NextToken(T);

    return (true);
}

static bool ParseQuaternion(Tokenizer* T, AxQuat* Quaternion, SceneParser* SP)
{
    if (!ExpectToken(T, AX_TOKEN_NUMBER)) {
        SP->SetParserError(T, "Expected number for quaternion X at line %d", T->CurrentToken.Line);
        return (false);
    }
    if (!ParseFloatToken(&T->CurrentToken, &Quaternion->X)) { return (false); }
    T->CurrentToken = NextToken(T);

    if (!ExpectToken(T, AX_TOKEN_COMMA)) { return (false); }
    T->CurrentToken = NextToken(T);

    if (!ExpectToken(T, AX_TOKEN_NUMBER)) { return (false); }
    if (!ParseFloatToken(&T->CurrentToken, &Quaternion->Y)) { return (false); }
    T->CurrentToken = NextToken(T);

    if (!ExpectToken(T, AX_TOKEN_COMMA)) { return (false); }
    T->CurrentToken = NextToken(T);

    if (!ExpectToken(T, AX_TOKEN_NUMBER)) { return (false); }
    if (!ParseFloatToken(&T->CurrentToken, &Quaternion->Z)) { return (false); }
    T->CurrentToken = NextToken(T);

    if (!ExpectToken(T, AX_TOKEN_COMMA)) { return (false); }
    T->CurrentToken = NextToken(T);

    if (!ExpectToken(T, AX_TOKEN_NUMBER)) { return (false); }
    if (!ParseFloatToken(&T->CurrentToken, &Quaternion->W)) { return (false); }
    T->CurrentToken = NextToken(T);

    return (true);
}

// Parses a "transform { ... }" block.
static bool ParseTransform(Tokenizer* T, AxTransform* Transform, SceneParser* SP)
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

    if (!ExpectToken(T, AX_TOKEN_LBRACE)) {
        SP->SetParserError(T, "Expected '{' to start transform block at line %d", T->CurrentToken.Line);
        return (false);
    }
    T->CurrentToken = NextToken(T);

    while (T->CurrentToken.Type != AX_TOKEN_RBRACE &&
           T->CurrentToken.Type != AX_TOKEN_EOF) {
        if (!ExpectToken(T, AX_TOKEN_IDENTIFIER)) {
            SP->SetParserError(T, "Expected transform property at line %d", T->CurrentToken.Line);
            return (false);
        }

        if (TokenEquals(&T->CurrentToken, "translation")) {
            T->CurrentToken = NextToken(T);
            if (!ExpectToken(T, AX_TOKEN_COLON)) { return (false); }
            T->CurrentToken = NextToken(T);
            if (!ParseVector3(T, &Transform->Translation, SP)) { return (false); }
        } else if (TokenEquals(&T->CurrentToken, "rotation")) {
            T->CurrentToken = NextToken(T);
            if (!ExpectToken(T, AX_TOKEN_COLON)) { return (false); }
            T->CurrentToken = NextToken(T);
            if (!ParseQuaternion(T, &Transform->Rotation, SP)) { return (false); }
        } else if (TokenEquals(&T->CurrentToken, "scale")) {
            T->CurrentToken = NextToken(T);
            if (!ExpectToken(T, AX_TOKEN_COLON)) { return (false); }
            T->CurrentToken = NextToken(T);
            if (!ParseVector3(T, &Transform->Scale, SP)) { return (false); }
        } else {
            SP->SetParserError(T, "Unknown transform property at line %d", T->CurrentToken.Line);
            return (false);
        }

        while (T->CurrentToken.Type == AX_TOKEN_COMMENT ||
               T->CurrentToken.Type == AX_TOKEN_NEWLINE) {
            T->CurrentToken = NextToken(T);
        }
    }

    if (!ExpectToken(T, AX_TOKEN_RBRACE)) {
        SP->SetParserError(T, "Expected '}' to close transform block at line %d", T->CurrentToken.Line);
        return (false);
    }
    T->CurrentToken = NextToken(T);
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
                             Tokenizer* T)
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
            } else if (strcmp(PropName, "color") == 0) {
                // color is parsed as vector3 inline; handled at parse time
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
// Deep Copy Helper (file-local)
///////////////////////////////////////////////////////////////

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

///////////////////////////////////////////////////////////////
// SceneParser Private Method Implementations
///////////////////////////////////////////////////////////////

void SceneParser::SetParserError(Tokenizer* T, const char* Format, ...)
{
    va_list Args;
    va_start(Args, Format);
    vsnprintf(T->ErrorMessage, sizeof(T->ErrorMessage), Format, Args);
    va_end(Args);

    strncpy(ParserLastErrorMessage_, T->ErrorMessage, sizeof(ParserLastErrorMessage_) - 1);
    ParserLastErrorMessage_[sizeof(ParserLastErrorMessage_) - 1] = '\0';
}

Node* SceneParser::ParseNodeImpl(Tokenizer* T, SceneTree* Scene, Node* Parent)
{
    // Called with T->CurrentToken == "node" identifier.
    T->CurrentToken = NextToken(T); // advance past "node"

    if (!ExpectToken(T, AX_TOKEN_STRING)) {
        SetParserError(T, "Expected node name string at line %d", T->CurrentToken.Line);
        return (nullptr);
    }

    char NodeName[64];
    CopyTokenString(&T->CurrentToken, NodeName, sizeof(NodeName));
    T->CurrentToken = NextToken(T);

    // Check for optional type name identifier before '{'
    NodeType Type = NodeType::Node3D; // default if no type specified
    while (T->CurrentToken.Type == AX_TOKEN_COMMENT ||
           T->CurrentToken.Type == AX_TOKEN_NEWLINE) {
        T->CurrentToken = NextToken(T);
    }

    if (T->CurrentToken.Type == AX_TOKEN_IDENTIFIER) {
        char TypeName[64];
        CopyTokenString(&T->CurrentToken, TypeName, sizeof(TypeName));

        NodeType MappedType = MapTypeNameToNodeType(TypeName);
        if (MappedType != NodeType::Base) {
            Type = MappedType;
            T->CurrentToken = NextToken(T);
        } else {
            SetParserError(T, "Unknown node type '%s' at line %d",
                           TypeName, T->CurrentToken.Line);
            return (nullptr);
        }
    }

    if (!ExpectToken(T, AX_TOKEN_LBRACE)) {
        SetParserError(T, "Expected '{' to start node block at line %d", T->CurrentToken.Line);
        return (nullptr);
    }
    T->CurrentToken = NextToken(T);

    Node* NewNode = Scene->CreateNode(NodeName, Type, Parent);
    if (!NewNode) {
        SetParserError(T, "Failed to create node '%s'", NodeName);
        return (nullptr);
    }

    while (T->CurrentToken.Type != AX_TOKEN_RBRACE &&
           T->CurrentToken.Type != AX_TOKEN_EOF) {
        if (T->CurrentToken.Type == AX_TOKEN_COMMENT ||
            T->CurrentToken.Type == AX_TOKEN_NEWLINE) {
            T->CurrentToken = NextToken(T);
            continue;
        }

        if (!ExpectToken(T, AX_TOKEN_IDENTIFIER)) {
            SetParserError(T, "Expected keyword in node block at line %d", T->CurrentToken.Line);
            return (nullptr);
        }

        if (TokenEquals(&T->CurrentToken, "transform")) {
            T->CurrentToken = NextToken(T); // advance past "transform"
            AxTransform Transform;
            if (!ParseTransform(T, &Transform, this)) {
                return (nullptr);
            }
            NewNode->GetTransform() = Transform;
        } else if (TokenEquals(&T->CurrentToken, "node")) {
            if (!ParseNodeImpl(T, Scene, NewNode)) {
                return (nullptr);
            }
        } else {
            // Inline property: key: value
            char PropName[64];
            CopyTokenString(&T->CurrentToken, PropName, sizeof(PropName));
            T->CurrentToken = NextToken(T);

            if (!ExpectToken(T, AX_TOKEN_COLON)) {
                SetParserError(T, "Expected ':' after property name '%s' at line %d",
                               PropName, T->CurrentToken.Line);
                return (nullptr);
            }
            T->CurrentToken = NextToken(T);

            // Handle vector3 properties for Light nodes
            if (Type == NodeType::Light) {
                if (strcmp(PropName, "color") == 0) {
                    LightNode* LN = static_cast<LightNode*>(NewNode);
                    if (!ParseVector3(T, &LN->Light.Color, this)) { return (nullptr); }
                    continue;
                }
            }

            char ValueStr[256] = {0};
            float NumericValue = 0.0f;
            bool IsString = false;
            bool IsNumeric = false;

            // Skip whitespace tokens before the value
            while (T->CurrentToken.Type == AX_TOKEN_COMMENT ||
                   T->CurrentToken.Type == AX_TOKEN_NEWLINE) {
                T->CurrentToken = NextToken(T);
            }

            if (T->CurrentToken.Type == AX_TOKEN_STRING) {
                CopyTokenString(&T->CurrentToken, ValueStr, sizeof(ValueStr));
                IsString = true;
                T->CurrentToken = NextToken(T);
            } else if (T->CurrentToken.Type == AX_TOKEN_NUMBER) {
                ParseFloatToken(&T->CurrentToken, &NumericValue);
                IsNumeric = true;
                T->CurrentToken = NextToken(T);
            } else if (T->CurrentToken.Type == AX_TOKEN_IDENTIFIER) {
                CopyTokenString(&T->CurrentToken, ValueStr, sizeof(ValueStr));
                T->CurrentToken = NextToken(T);
            }

            SetNodeProperty(NewNode, Type, PropName, ValueStr, NumericValue, IsString, IsNumeric, T);
        }
    }

    if (!ExpectToken(T, AX_TOKEN_RBRACE)) {
        SetParserError(T, "Expected '}' to close node block at line %d", T->CurrentToken.Line);
        return (nullptr);
    }
    T->CurrentToken = NextToken(T);

    // For Light nodes, invoke OnLightParsed with the AxLight data
    if (Type == NodeType::Light) {
        LightNode* LN = static_cast<LightNode*>(NewNode);
        InvokeOnLightParsed(&LN->Light);
    }

    InvokeOnNodeParsed(NewNode);
    return (NewNode);
}

SceneTree* SceneParser::ParseSceneImpl(Tokenizer* T)
{
    if (!ExpectToken(T, AX_TOKEN_IDENTIFIER) ||
        !TokenEquals(&T->CurrentToken, "scene")) {
        SetParserError(T, "Expected 'scene' keyword at line %d", T->CurrentToken.Line);
        return (nullptr);
    }
    T->CurrentToken = NextToken(T);

    if (!ExpectToken(T, AX_TOKEN_STRING)) {
        SetParserError(T, "Expected scene name string at line %d", T->CurrentToken.Line);
        return (nullptr);
    }

    char SceneName[64];
    CopyTokenString(&T->CurrentToken, SceneName, sizeof(SceneName));
    T->CurrentToken = NextToken(T);

    if (!ExpectToken(T, AX_TOKEN_LBRACE)) {
        SetParserError(T, "Expected '{' to start scene block at line %d", T->CurrentToken.Line);
        return (nullptr);
    }
    T->CurrentToken = NextToken(T);

    SceneTree* Scene = new SceneTree(HashTableAPI_, T->Allocator);
    Scene->Name = SceneName;

    while (T->CurrentToken.Type != AX_TOKEN_RBRACE &&
           T->CurrentToken.Type != AX_TOKEN_EOF) {
        if (T->CurrentToken.Type == AX_TOKEN_COMMENT ||
            T->CurrentToken.Type == AX_TOKEN_NEWLINE) {
            T->CurrentToken = NextToken(T);
            continue;
        }

        if (!ExpectToken(T, AX_TOKEN_IDENTIFIER)) {
            SetParserError(T, "Expected keyword at line %d", T->CurrentToken.Line);
            delete Scene;
            return (nullptr);
        }

        if (TokenEquals(&T->CurrentToken, "node")) {
            if (!ParseNodeImpl(T, Scene, nullptr)) {
                delete Scene;
                return (nullptr);
            }
        } else {
            SetParserError(T, "Unknown scene keyword '%.*s' at line %d",
                           static_cast<int>(T->CurrentToken.Length),
                           T->CurrentToken.Start, T->CurrentToken.Line);
            delete Scene;
            return (nullptr);
        }
    }

    if (!ExpectToken(T, AX_TOKEN_RBRACE)) {
        SetParserError(T, "Expected '}' to close scene block at line %d", T->CurrentToken.Line);
        delete Scene;
        return (nullptr);
    }

    InvokeOnSceneParsed(Scene);
    return (Scene);
}

///////////////////////////////////////////////////////////////
// SceneParser Public Method Implementations
///////////////////////////////////////////////////////////////

void SceneParser::Init(AxAPIRegistry* Registry)
{
    if (!Registry) {
        return;
    }

    AllocatorAPI_ = static_cast<AxAllocatorAPI*>(Registry->Get(AXON_ALLOCATOR_API_NAME));
    PlatformAPI_  = static_cast<AxPlatformAPI*>(Registry->Get(AXON_PLATFORM_API_NAME));
    HashTableAPI_ = static_cast<AxHashTableAPI*>(Registry->Get(AXON_HASH_TABLE_API_NAME));
}

void SceneParser::Term()
{
    AllocatorAPI_  = nullptr;
    PlatformAPI_   = nullptr;
    HashTableAPI_  = nullptr;
}

SceneTree* SceneParser::ParseFromString(const char* Source, AxAllocator* Allocator)
{
    if (!Source || !Allocator) {
        strncpy(ParserLastErrorMessage_, "Invalid parameters: Source and Allocator cannot be NULL",
                sizeof(ParserLastErrorMessage_) - 1);
        return (nullptr);
    }

    Tokenizer T;
    InitTokenizer(&T, Source, Allocator);

    while (T.CurrentToken.Type == AX_TOKEN_COMMENT ||
           T.CurrentToken.Type == AX_TOKEN_NEWLINE) {
        T.CurrentToken = NextToken(&T);
    }

    if (T.CurrentToken.Type == AX_TOKEN_EOF) {
        SetParserError(&T, "Empty scene file");
        return (nullptr);
    }

    return (ParseSceneImpl(&T));
}

SceneTree* SceneParser::ParseFromFile(const char* FilePath, AxAllocator* Allocator)
{
    if (!FilePath || !Allocator) {
        strncpy(ParserLastErrorMessage_, "Invalid parameters: FilePath and Allocator cannot be NULL",
                sizeof(ParserLastErrorMessage_) - 1);
        return (nullptr);
    }

    const AxPlatformFileAPI* FileAPI = PlatformAPI_->FileAPI;
    AxFile File = FileAPI->OpenForRead(FilePath);
    if (!FileAPI->IsValid(File)) {
        snprintf(ParserLastErrorMessage_, sizeof(ParserLastErrorMessage_),
                 "Failed to open file: %s", FilePath);
        return (nullptr);
    }

    uint64_t FileSize = FileAPI->Size(File);
    if (FileSize == 0) {
        FileAPI->Close(File);
        snprintf(ParserLastErrorMessage_, sizeof(ParserLastErrorMessage_),
                 "File is empty: %s", FilePath);
        return (nullptr);
    }

    char* FileContent = static_cast<char*>(AxAlloc(Allocator, FileSize + 1));
    if (!FileContent) {
        FileAPI->Close(File);
        strncpy(ParserLastErrorMessage_, "Failed to allocate memory for file content",
                sizeof(ParserLastErrorMessage_) - 1);
        return (nullptr);
    }

    uint64_t BytesRead = FileAPI->Read(File, FileContent, FileSize);
    FileAPI->Close(File);

    if (BytesRead != FileSize) {
        snprintf(ParserLastErrorMessage_, sizeof(ParserLastErrorMessage_),
                 "Failed to read complete file: %s", FilePath);
        return (nullptr);
    }

    FileContent[FileSize] = '\0';
    return (ParseFromString(FileContent, Allocator));
}

SceneTree* SceneParser::LoadSceneFromFile(const char* FilePath)
{
    if (!FilePath) {
        SetError("Invalid file path: NULL");
        return (nullptr);
    }

    AxAllocator* SceneAllocator = AllocatorAPI_->CreateLinear("ScenePersistent", DefaultSceneMemorySize_);
    if (!SceneAllocator) {
        SetError("Failed to create scene allocator");
        return (nullptr);
    }

    SceneTree* Scene = ParseFromFile(FilePath, SceneAllocator);
    if (!Scene) {
        SceneAllocator->Destroy(SceneAllocator);
        const char* ParseError = ParserLastErrorMessage_[0] != '\0' ? ParserLastErrorMessage_ : nullptr;
        SetError("Failed to parse scene file '%s': %s", FilePath, ParseError ? ParseError : "Unknown error");
        return (nullptr);
    }

    return (Scene);
}

SceneTree* SceneParser::LoadSceneFromString(const char* SceneData)
{
    if (!SceneData) {
        SetError("Invalid scene data: NULL");
        return (nullptr);
    }

    AxAllocator* SceneAllocator = AllocatorAPI_->CreateLinear("ScenePersistent", DefaultSceneMemorySize_);
    if (!SceneAllocator) {
        SetError("Failed to create scene allocator");
        return (nullptr);
    }

    SceneTree* Scene = ParseFromString(SceneData, SceneAllocator);
    if (!Scene) {
        SceneAllocator->Destroy(SceneAllocator);
        const char* ParseError = ParserLastErrorMessage_[0] != '\0' ? ParserLastErrorMessage_ : nullptr;
        SetError("Failed to parse scene data: %s", ParseError ? ParseError : "Unknown error");
        return (nullptr);
    }

    return (Scene);
}

Node* SceneParser::ParsePrefab(const char* PrefabData, SceneTree* Scene)
{
    if (!PrefabData || !Scene) {
        return (nullptr);
    }

    AxAllocator* Allocator = Scene->Allocator;
    if (!Allocator && AllocatorAPI_) {
        Allocator = AllocatorAPI_->CreateHeap("PrefabAlloc", 64 * 1024, 256 * 1024);
    }
    if (!Allocator) {
        return (nullptr);
    }

    Tokenizer T;
    InitTokenizer(&T, PrefabData, Allocator);

    while (T.CurrentToken.Type == AX_TOKEN_COMMENT ||
           T.CurrentToken.Type == AX_TOKEN_NEWLINE) {
        T.CurrentToken = NextToken(&T);
    }

    if (!ExpectToken(&T, AX_TOKEN_IDENTIFIER) ||
        !TokenEquals(&T.CurrentToken, "node")) {
        return (nullptr);
    }

    return (ParseNodeImpl(&T, Scene, nullptr));
}

Node* SceneParser::InstantiatePrefab(SceneTree* Scene, Node* PrefabRoot, Node* Parent)
{
    if (!Scene || !PrefabRoot) {
        return (nullptr);
    }

    return (DeepCopyNode(Scene, PrefabRoot, Parent));
}

void SceneParser::SetDefaultSceneMemorySize(size_t MemorySize)
{
    if (MemorySize > 0) {
        DefaultSceneMemorySize_ = MemorySize;
    }
}

size_t SceneParser::GetDefaultSceneMemorySize() const
{
    return (DefaultSceneMemorySize_);
}

const char* SceneParser::GetLastError() const
{
    return (LastErrorMessage_[0] != '\0' ? LastErrorMessage_ : nullptr);
}

AxSceneResult SceneParser::RegisterEventHandler(const AxSceneEvents* Events)
{
    if (!Events) {
        SetError("Cannot register NULL event handler");
        return (AX_SCENE_ERROR_INVALID_ARGUMENTS);
    }

    if (EventHandlerCount_ >= MaxEventHandlers_) {
        SetError("Maximum number of event handlers (%d) reached", MaxEventHandlers_);
        return (AX_SCENE_ERROR_OUT_OF_MEMORY);
    }

    for (int i = 0; i < EventHandlerCount_; i++) {
        if (memcmp(&EventHandlers_[i], Events, sizeof(AxSceneEvents)) == 0) {
            SetError("Event handler already registered");
            return (AX_SCENE_ERROR_ALREADY_EXISTS);
        }
    }

    EventHandlers_[EventHandlerCount_] = *Events;
    EventHandlerCount_++;
    return (AX_SCENE_SUCCESS);
}

AxSceneResult SceneParser::UnregisterEventHandler(const AxSceneEvents* Events)
{
    if (!Events) {
        SetError("Cannot unregister NULL event handler");
        return (AX_SCENE_ERROR_INVALID_ARGUMENTS);
    }

    for (int i = 0; i < EventHandlerCount_; i++) {
        if (memcmp(&EventHandlers_[i], Events, sizeof(AxSceneEvents)) == 0) {
            for (int j = i; j < EventHandlerCount_ - 1; j++) {
                EventHandlers_[j] = EventHandlers_[j + 1];
            }
            EventHandlerCount_--;
            return (AX_SCENE_SUCCESS);
        }
    }

    SetError("Event handler not found in registry");
    return (AX_SCENE_ERROR_NOT_FOUND);
}

void SceneParser::ClearEventHandlers()
{
    EventHandlerCount_ = 0;
    memset(EventHandlers_, 0, sizeof(EventHandlers_));
}

void SceneParser::SetError(const char* Format, ...)
{
    va_list Args;
    va_start(Args, Format);
    vsnprintf(LastErrorMessage_, sizeof(LastErrorMessage_), Format, Args);
    va_end(Args);
}

void SceneParser::InvokeOnLightParsed(const AxLight* Light)
{
    for (int i = 0; i < EventHandlerCount_; ++i) {
        if (EventHandlers_[i].OnLightParsed) {
            EventHandlers_[i].OnLightParsed(Light, EventHandlers_[i].UserData);
        }
    }
}

void SceneParser::InvokeOnSceneParsed(const SceneTree* Scene)
{
    for (int i = 0; i < EventHandlerCount_; ++i) {
        if (EventHandlers_[i].OnSceneParsed) {
            EventHandlers_[i].OnSceneParsed(Scene, EventHandlers_[i].UserData);
        }
    }
}

void SceneParser::InvokeOnNodeParsed(Node* ParsedNode)
{
    for (int i = 0; i < EventHandlerCount_; ++i) {
        if (EventHandlers_[i].OnNodeParsed) {
            EventHandlers_[i].OnNodeParsed(
                static_cast<void*>(ParsedNode), EventHandlers_[i].UserData);
        }
    }
}
