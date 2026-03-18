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
#include "AxEngine/AxPrimitives.h"
#include "AxEngine/AxPropertyReflection.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cctype>
#include <string>

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
// Reflection-Driven Property Setting
///////////////////////////////////////////////////////////////

/**
 * Parse a property value from the tokenizer and set it on the node.
 * Each PropType owns its own token consumption, so multi-token types
 * (Vec3, Vec4, Quat) are handled uniformly alongside single-token types.
 * Returns false on parse error, true on success (including unknown properties).
 */
static bool ParseAndSetProperty(Node* NodePtr, const PropDescriptor* Desc,
                                Tokenizer* T, SceneParser* Parser)
{
    switch (Desc->Type) {
        case PropType::Float: {
            if (T->CurrentToken.Type != AX_TOKEN_NUMBER) { return (true); }
            float V;
            ParseFloatToken(&T->CurrentToken, &V);
            T->CurrentToken = NextToken(T);
            SetPropertyFloat(NodePtr, Desc, V);
            return (true);
        }
        case PropType::Int32: {
            if (T->CurrentToken.Type != AX_TOKEN_NUMBER) { return (true); }
            float V;
            ParseFloatToken(&T->CurrentToken, &V);
            T->CurrentToken = NextToken(T);
            SetPropertyInt32(NodePtr, Desc, static_cast<int32_t>(V));
            return (true);
        }
        case PropType::UInt32: {
            if (T->CurrentToken.Type != AX_TOKEN_NUMBER) { return (true); }
            float V;
            ParseFloatToken(&T->CurrentToken, &V);
            T->CurrentToken = NextToken(T);
            SetPropertyUInt32(NodePtr, Desc, static_cast<uint32_t>(V));
            return (true);
        }
        case PropType::Bool: {
            if (T->CurrentToken.Type != AX_TOKEN_IDENTIFIER) { return (true); }
            char Buf[16];
            CopyTokenString(&T->CurrentToken, Buf, sizeof(Buf));
            T->CurrentToken = NextToken(T);
            SetPropertyBool(NodePtr, Desc, strcmp(Buf, "true") == 0);
            return (true);
        }
        case PropType::String: {
            if (T->CurrentToken.Type != AX_TOKEN_STRING) { return (true); }
            char Buf[256];
            CopyTokenString(&T->CurrentToken, Buf, sizeof(Buf));
            T->CurrentToken = NextToken(T);
            SetPropertyString(NodePtr, Desc, std::string(Buf));
            return (true);
        }
        case PropType::Enum: {
            if (T->CurrentToken.Type != AX_TOKEN_IDENTIFIER) { return (true); }
            char Buf[64];
            CopyTokenString(&T->CurrentToken, Buf, sizeof(Buf));
            T->CurrentToken = NextToken(T);
            SetPropertyEnum(NodePtr, Desc, Buf);
            return (true);
        }
        case PropType::Vec3: {
            AxVec3 V;
            if (!ParseVector3(T, &V, Parser)) { return (false); }
            SetPropertyVec3(NodePtr, Desc, Vec3(V));
            return (true);
        }
        case PropType::Vec4:
        case PropType::Quat:
            return (true);
    }
    return (true);
}

///////////////////////////////////////////////////////////////
// Primitive Mesh Parsing Helper
///////////////////////////////////////////////////////////////

/**
 * Parse a primitive mesh declaration from the token stream.
 * Called after the "primitive" identifier has been consumed.
 *
 * Expected syntax: <shape> { key: value ... }
 * or: <shape> { }
 *
 * Reads the shape name, optional parameter block, calls the
 * appropriate primitive creation function, and assigns the result
 * to the MeshInstance's ModelHandle.
 */
static bool ParsePrimitiveMesh(Tokenizer* T, MeshInstance* MI, SceneParser* SP)
{
    // Read shape name identifier
    if (!ExpectToken(T, AX_TOKEN_IDENTIFIER)) {
        SP->SetParserError(T, "Expected primitive shape name at line %d", T->CurrentToken.Line);
        return (false);
    }

    char ShapeName[32];
    CopyTokenString(&T->CurrentToken, ShapeName, sizeof(ShapeName));
    T->CurrentToken = NextToken(T);

    // Clear MeshPath to indicate this is a primitive, not file-based
    MI->MeshPath = "";

    // Initialize default primitive mesh instances for each shape
    BoxMesh BoxP;
    SphereMesh SphereP;
    PlaneMesh PlaneP;
    CylinderMesh CylP;
    CapsuleMesh CapP;

    // Parse optional parameter block { key: value ... }
    // Skip whitespace/comments first
    while (T->CurrentToken.Type == AX_TOKEN_COMMENT ||
           T->CurrentToken.Type == AX_TOKEN_NEWLINE) {
        T->CurrentToken = NextToken(T);
    }

    if (T->CurrentToken.Type == AX_TOKEN_LBRACE) {
        T->CurrentToken = NextToken(T); // consume '{'

        while (T->CurrentToken.Type != AX_TOKEN_RBRACE &&
               T->CurrentToken.Type != AX_TOKEN_EOF) {
            // Skip whitespace tokens
            if (T->CurrentToken.Type == AX_TOKEN_COMMENT ||
                T->CurrentToken.Type == AX_TOKEN_NEWLINE) {
                T->CurrentToken = NextToken(T);
                continue;
            }

            if (T->CurrentToken.Type != AX_TOKEN_IDENTIFIER) {
                SP->SetParserError(T, "Expected parameter name in primitive block at line %d",
                                   T->CurrentToken.Line);
                return (false);
            }

            char ParamName[64];
            CopyTokenString(&T->CurrentToken, ParamName, sizeof(ParamName));
            T->CurrentToken = NextToken(T);

            // Expect colon
            if (!ExpectToken(T, AX_TOKEN_COLON)) {
                SP->SetParserError(T, "Expected ':' after parameter '%s' at line %d",
                                   ParamName, T->CurrentToken.Line);
                return (false);
            }
            T->CurrentToken = NextToken(T);

            // Read value (number or identifier like true/false)
            while (T->CurrentToken.Type == AX_TOKEN_COMMENT ||
                   T->CurrentToken.Type == AX_TOKEN_NEWLINE) {
                T->CurrentToken = NextToken(T);
            }

            float FloatVal = 0.0f;
            bool GotFloat = false;
            char IdentVal[32] = {};

            if (T->CurrentToken.Type == AX_TOKEN_NUMBER) {
                ParseFloatToken(&T->CurrentToken, &FloatVal);
                GotFloat = true;
                T->CurrentToken = NextToken(T);
            } else if (T->CurrentToken.Type == AX_TOKEN_IDENTIFIER) {
                CopyTokenString(&T->CurrentToken, IdentVal, sizeof(IdentVal));
                T->CurrentToken = NextToken(T);
            } else {
                SP->SetParserError(T, "Expected value for parameter '%s' at line %d",
                                   ParamName, T->CurrentToken.Line);
                return (false);
            }

            // Assign parameter to the appropriate shape via fluent setters
            if (strcmp(ShapeName, "box") == 0) {
                if (strcmp(ParamName, "width") == 0 && GotFloat)       { BoxP.SetWidth(FloatVal); }
                else if (strcmp(ParamName, "height") == 0 && GotFloat) { BoxP.SetHeight(FloatVal); }
                else if (strcmp(ParamName, "depth") == 0 && GotFloat)  { BoxP.SetDepth(FloatVal); }
            } else if (strcmp(ShapeName, "sphere") == 0) {
                if (strcmp(ParamName, "radius") == 0 && GotFloat)        { SphereP.SetRadius(FloatVal); }
                else if (strcmp(ParamName, "rings") == 0 && GotFloat)    { SphereP.SetRings(static_cast<uint32_t>(FloatVal)); }
                else if (strcmp(ParamName, "segments") == 0 && GotFloat) { SphereP.SetSegments(static_cast<uint32_t>(FloatVal)); }
            } else if (strcmp(ShapeName, "plane") == 0) {
                if (strcmp(ParamName, "width") == 0 && GotFloat)              { PlaneP.SetWidth(FloatVal); }
                else if (strcmp(ParamName, "depth") == 0 && GotFloat)         { PlaneP.SetDepth(FloatVal); }
                else if (strcmp(ParamName, "subdivisionsX") == 0 && GotFloat) { PlaneP.SetSubdivisionsX(static_cast<uint32_t>(FloatVal)); }
                else if (strcmp(ParamName, "subdivisionsZ") == 0 && GotFloat) { PlaneP.SetSubdivisionsZ(static_cast<uint32_t>(FloatVal)); }
            } else if (strcmp(ShapeName, "cylinder") == 0) {
                if (strcmp(ParamName, "radius") == 0 && GotFloat)        { CylP.SetRadius(FloatVal); }
                else if (strcmp(ParamName, "height") == 0 && GotFloat)   { CylP.SetHeight(FloatVal); }
                else if (strcmp(ParamName, "segments") == 0 && GotFloat) { CylP.SetSegments(static_cast<uint32_t>(FloatVal)); }
                else if (strcmp(ParamName, "capTop") == 0)    { CylP.SetCapTop(strcmp(IdentVal, "true") == 0); }
                else if (strcmp(ParamName, "capBottom") == 0) { CylP.SetCapBottom(strcmp(IdentVal, "true") == 0); }
            } else if (strcmp(ShapeName, "capsule") == 0) {
                if (strcmp(ParamName, "radius") == 0 && GotFloat)        { CapP.SetRadius(FloatVal); }
                else if (strcmp(ParamName, "height") == 0 && GotFloat)   { CapP.SetHeight(FloatVal); }
                else if (strcmp(ParamName, "rings") == 0 && GotFloat)    { CapP.SetRings(static_cast<uint32_t>(FloatVal)); }
                else if (strcmp(ParamName, "segments") == 0 && GotFloat) { CapP.SetSegments(static_cast<uint32_t>(FloatVal)); }
            }
        }

        // Expect closing brace
        if (!ExpectToken(T, AX_TOKEN_RBRACE)) {
            SP->SetParserError(T, "Expected '}' to close primitive parameter block at line %d",
                               T->CurrentToken.Line);
            return (false);
        }
        T->CurrentToken = NextToken(T);
    }

    // Call CreateModel() on the appropriate primitive mesh instance
    {
        AxModelHandle Handle = AX_INVALID_HANDLE;

        if (strcmp(ShapeName, "box") == 0) {
            Handle = BoxP.CreateModel();
        } else if (strcmp(ShapeName, "sphere") == 0) {
            Handle = SphereP.CreateModel();
        } else if (strcmp(ShapeName, "plane") == 0) {
            Handle = PlaneP.CreateModel();
        } else if (strcmp(ShapeName, "cylinder") == 0) {
            Handle = CylP.CreateModel();
        } else if (strcmp(ShapeName, "capsule") == 0) {
            Handle = CapP.CreateModel();
        }

        MI->ModelHandle = Handle;
    }

    return (true);
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

            // Skip whitespace tokens before the value
            while (T->CurrentToken.Type == AX_TOKEN_COMMENT ||
                   T->CurrentToken.Type == AX_TOKEN_NEWLINE) {
                T->CurrentToken = NextToken(T);
            }

            // Check for primitive mesh syntax: mesh: primitive <shape> { params }
            if (Type == NodeType::MeshInstance &&
                strcmp(PropName, "mesh") == 0 &&
                T->CurrentToken.Type == AX_TOKEN_IDENTIFIER &&
                TokenEquals(&T->CurrentToken, "primitive")) {
                T->CurrentToken = NextToken(T); // consume "primitive"

                MeshInstance* MI = static_cast<MeshInstance*>(NewNode);
                if (!ParsePrimitiveMesh(T, MI, this)) {
                    return (nullptr);
                }
                continue;
            }

            // Look up property descriptor and parse+set in one step
            const PropDescriptor* Desc = PropertyRegistry::Get().FindProperty(Type, PropName);
            if (Desc) {
                if (!ParseAndSetProperty(NewNode, Desc, T, this)) {
                    return (nullptr);
                }
            }
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
        AxLight TempLight = LN->BuildLight();
        InvokeOnLightParsed(&TempLight);
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

    Registry_     = Registry;
    AllocatorAPI_ = static_cast<AxAllocatorAPI*>(Registry->Get(AXON_ALLOCATOR_API_NAME));
    PlatformAPI_  = static_cast<AxPlatformAPI*>(Registry->Get(AXON_PLATFORM_API_NAME));
    HashTableAPI_ = static_cast<AxHashTableAPI*>(Registry->Get(AXON_HASH_TABLE_API_NAME));
}

void SceneParser::Term()
{
    Registry_      = nullptr;
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

///////////////////////////////////////////////////////////////
// Scene Serialization (Save)
///////////////////////////////////////////////////////////////

/**
 * Map a NodeType enum to the type name string used in .ats files.
 * Returns nullptr for types that should not appear in serialization
 * (Root, Base).
 */
static const char* NodeTypeToTypeName(NodeType Type)
{
    switch (Type) {
        case NodeType::MeshInstance:    return ("MeshInstance");
        case NodeType::Camera:         return ("Camera");
        case NodeType::Light:          return ("Light");
        case NodeType::RigidBody:      return ("RigidBody");
        case NodeType::Collider:       return ("Collider");
        case NodeType::AudioSource:    return ("AudioSource");
        case NodeType::AudioListener:  return ("AudioListener");
        case NodeType::Animator:       return ("Animator");
        case NodeType::ParticleEmitter: return ("ParticleEmitter");
        case NodeType::Sprite:         return ("Sprite");
        case NodeType::Node3D:         return ("Node3D");
        case NodeType::Node2D:         return ("Node2D");
        default:                       return (nullptr);
    }
}

/**
 * Append indentation (4 spaces per level) to the output string.
 */
static void AppendIndent(std::string& Output, int Indent)
{
    for (int i = 0; i < Indent; ++i) {
        Output += "    ";
    }
}

/**
 * Format a float, stripping unnecessary trailing zeros.
 * Ensures at least one decimal place (e.g. "1.0" not "1").
 */
static std::string FormatFloat(float Value)
{
    char Buffer[64];
    snprintf(Buffer, sizeof(Buffer), "%.6f", Value);

    // Strip trailing zeros after decimal point, keeping at least one digit
    char* Dot = strchr(Buffer, '.');
    if (Dot) {
        char* End = Buffer + strlen(Buffer) - 1;
        while (End > Dot + 1 && *End == '0') {
            *End = '\0';
            --End;
        }
    }

    return (std::string(Buffer));
}

/**
 * Serialize the transform block for a node.
 * Only writes non-default values to keep output clean.
 */
static void SerializeTransform(std::string& Output, const AxTransform& Transform, int Indent)
{
    bool HasTranslation = (Transform.Translation.X != 0.0f ||
                           Transform.Translation.Y != 0.0f ||
                           Transform.Translation.Z != 0.0f);
    bool HasRotation = (Transform.Rotation.X != 0.0f ||
                        Transform.Rotation.Y != 0.0f ||
                        Transform.Rotation.Z != 0.0f ||
                        Transform.Rotation.W != 1.0f);
    bool HasScale = (Transform.Scale.X != 1.0f ||
                     Transform.Scale.Y != 1.0f ||
                     Transform.Scale.Z != 1.0f);

    if (!HasTranslation && !HasRotation && !HasScale) {
        return;
    }

    AppendIndent(Output, Indent);
    Output += "transform {\n";

    if (HasTranslation) {
        AppendIndent(Output, Indent + 1);
        Output += "translation: ";
        Output += FormatFloat(Transform.Translation.X) + ", ";
        Output += FormatFloat(Transform.Translation.Y) + ", ";
        Output += FormatFloat(Transform.Translation.Z) + "\n";
    }

    if (HasRotation) {
        AppendIndent(Output, Indent + 1);
        Output += "rotation: ";
        Output += FormatFloat(Transform.Rotation.X) + ", ";
        Output += FormatFloat(Transform.Rotation.Y) + ", ";
        Output += FormatFloat(Transform.Rotation.Z) + ", ";
        Output += FormatFloat(Transform.Rotation.W) + "\n";
    }

    if (HasScale) {
        AppendIndent(Output, Indent + 1);
        Output += "scale: ";
        Output += FormatFloat(Transform.Scale.X) + ", ";
        Output += FormatFloat(Transform.Scale.Y) + ", ";
        Output += FormatFloat(Transform.Scale.Z) + "\n";
    }

    AppendIndent(Output, Indent);
    Output += "}\n";
}

/**
 * Check if a property is at its default value (skip-if-default serialization).
 */
static bool IsPropertyAtDefault(const Node* N, const PropDescriptor* D)
{
    switch (D->Type) {
        case PropType::Float:  return (GetPropertyFloat(N, D) == D->FloatDefault);
        case PropType::Int32:  return (GetPropertyInt32(N, D) == D->IntDefault);
        case PropType::UInt32: return (GetPropertyUInt32(N, D) == D->UIntDefault);
        case PropType::Bool:   return (GetPropertyBool(N, D) == D->BoolDefault);
        case PropType::String: return (GetPropertyString(N, D).empty());
        case PropType::Enum:   return (GetPropertyInt32(N, D) == D->IntDefault);
        case PropType::Vec3:   return (false);  // always write Vec3
        case PropType::Vec4:   return (false);  // always write Vec4
        case PropType::Quat:   return (false);  // always write Quat
    }
    return (false);
}

/**
 * Serialize a single property value to the output string.
 */
static void SerializeProperty(std::string& Output, const Node* N, const PropDescriptor* D, int Indent)
{
    AppendIndent(Output, Indent);
    Output += D->Name;
    Output += ": ";

    switch (D->Type) {
        case PropType::Float:
            Output += FormatFloat(GetPropertyFloat(N, D));
            break;
        case PropType::Int32:
            Output += FormatFloat(static_cast<float>(GetPropertyInt32(N, D)));
            break;
        case PropType::UInt32:
            Output += FormatFloat(static_cast<float>(GetPropertyUInt32(N, D)));
            break;
        case PropType::Bool:
            Output += GetPropertyBool(N, D) ? "true" : "false";
            break;
        case PropType::String:
            Output += "\"";
            Output += GetPropertyString(N, D);
            Output += "\"";
            break;
        case PropType::Vec3: {
            Vec3 V = GetPropertyVec3(N, D);
            Output += FormatFloat(V.X) + ", ";
            Output += FormatFloat(V.Y) + ", ";
            Output += FormatFloat(V.Z);
            break;
        }
        case PropType::Vec4: {
            Vec4 V = GetPropertyVec4(N, D);
            Output += FormatFloat(V.X) + ", ";
            Output += FormatFloat(V.Y) + ", ";
            Output += FormatFloat(V.Z) + ", ";
            Output += FormatFloat(V.W);
            break;
        }
        case PropType::Enum: {
            const char* Str = EnumToString(D->EnumEntries, D->EnumCount, GetPropertyInt32(N, D));
            Output += Str ? Str : "unknown";
            break;
        }
        case PropType::Quat:
            break;
    }

    Output += "\n";
}

/**
 * Serialize typed-node properties using the PropertyRegistry.
 * Iterates over registered property descriptors, skipping defaults.
 */
static void SerializeTypedNodeProperties(std::string& Output, Node* Current, int Indent)
{
    uint32_t Count = 0;
    const PropDescriptor* Props = PropertyRegistry::Get().GetProperties(Current->GetType(), &Count);
    if (!Props) {
        return;
    }

    for (uint32_t I = 0; I < Count; ++I) {
        const PropDescriptor& D = Props[I];
        if (!HasFlag(D.Flags, PropFlags::Serialize)) { continue; }
        if (IsPropertyAtDefault(Current, &D)) { continue; }
        SerializeProperty(Output, Current, &D, Indent);
    }
}

void SceneParser::SerializeNode(std::string& Output, Node* Current, int Indent)
{
    if (!Current) {
        return;
    }

    // Skip the root node itself -- its children are top-level scene nodes
    if (Current->GetType() == NodeType::Root) {
        Node* Child = Current->GetFirstChild();
        while (Child) {
            SerializeNode(Output, Child, Indent);
            Child = Child->GetNextSibling();
        }
        return;
    }

    const char* TypeName = NodeTypeToTypeName(Current->GetType());
    if (!TypeName) {
        TypeName = "Node3D";
    }

    // Node header: node "Name" TypeName {
    AppendIndent(Output, Indent);
    Output += "node \"";
    Output += std::string(Current->GetName());
    Output += "\" ";
    Output += TypeName;
    Output += " {\n";

    // Transform block
    AxTransform AxT = Current->GetTransform();
    SerializeTransform(Output, AxT, Indent + 1);

    // Typed node properties
    SerializeTypedNodeProperties(Output, Current, Indent + 1);

    // Recurse to children (nested inside parent node block)
    Node* Child = Current->GetFirstChild();
    while (Child) {
        Output += "\n";
        SerializeNode(Output, Child, Indent + 1);
        Child = Child->GetNextSibling();
    }

    // Close node block
    AppendIndent(Output, Indent);
    Output += "}\n";
}

std::string SceneParser::SaveSceneToString(SceneTree* Scene)
{
    if (!Scene) {
        SetError("Cannot save NULL scene");
        return (std::string());
    }

    std::string Output;
    Output.reserve(4096);

    // Scene header
    Output += "scene \"";
    Output += Scene->Name.empty() ? "Untitled" : Scene->Name;
    Output += "\" {\n";

    // Serialize all children of the root node
    if (Scene->GetRootNode()) {
        SerializeNode(Output, static_cast<Node*>(Scene->GetRootNode()), 1);
    }

    Output += "}\n";

    return (Output);
}

bool SceneParser::SaveSceneToFile(SceneTree* Scene, const char* FilePath)
{
    if (!Scene || !FilePath) {
        SetError("Invalid arguments: Scene and FilePath cannot be NULL");
        return (false);
    }

    if (!PlatformAPI_) {
        SetError("PlatformAPI not available (SceneParser not initialized)");
        return (false);
    }

    std::string Content = SaveSceneToString(Scene);
    if (Content.empty()) {
        return (false);
    }

    const AxPlatformFileAPI* FileAPI = PlatformAPI_->FileAPI;
    AxFile File = FileAPI->OpenForWrite(FilePath);
    if (!FileAPI->IsValid(File)) {
        SetError("Failed to open file for writing: %s", FilePath);
        return (false);
    }

    uint64_t BytesWritten = FileAPI->Write(File,
        const_cast<void*>(static_cast<const void*>(Content.c_str())),
        static_cast<uint32_t>(Content.size()));
    FileAPI->Close(File);

    if (BytesWritten != Content.size()) {
        SetError("Failed to write complete scene to file: %s", FilePath);
        return (false);
    }

    return (true);
}
