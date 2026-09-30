#include "PCH.h"
#include "FArchiveJson.h"


FArchiveJson::FArchiveJson(rapidjson::Value& RootNode, rapidjson::Document::AllocatorType& InAllocator)
    : FArchive(EArchiveMode::Saving)
    , Allocator(&InAllocator)
{
    NodeStack.push_back(&RootNode);
}

FArchiveJson::FArchiveJson(rapidjson::Value& RootNode)
    : FArchive(EArchiveMode::Loading)
    , Allocator(nullptr)
{
    NodeStack.push_back(&RootNode);
}



// ---------------------------------------------------------
// Primitives
// ---------------------------------------------------------
#define IMPLEMENT_JSON_PRIMITIVE(Type, RapidIsFunc, RapidGetFunc) \
    void FArchiveJson::Serialize(std::string_view Name, Type& Value) \
    { \
        rapidjson::Value* Current = NodeStack.back(); if (!Current) return; \
        if (IsSaving()) { \
            rapidjson::Value JsonVal(Value); \
            AddChildNode(Current, Name, JsonVal); \
        } else { \
            rapidjson::Value* Child = GetChildNode(Current, Name); \
            if (Child && Child->RapidIsFunc()) Value = static_cast<Type>(Child->RapidGetFunc()); \
        } \
    }

IMPLEMENT_JSON_PRIMITIVE(bool, IsBool, GetBool)
IMPLEMENT_JSON_PRIMITIVE(int32, IsInt, GetInt)
IMPLEMENT_JSON_PRIMITIVE(uint32, IsUint, GetUint)
IMPLEMENT_JSON_PRIMITIVE(int64, IsInt64, GetInt64)
IMPLEMENT_JSON_PRIMITIVE(uint64, IsUint64, GetUint64)
// float32는 RapidJSON에서 Double로 다루는 것이 안전하므로 Number/Double로 매핑
IMPLEMENT_JSON_PRIMITIVE(float32, IsNumber, GetDouble)
IMPLEMENT_JSON_PRIMITIVE(float64, IsDouble, GetDouble)
#undef IMPLEMENT_JSON_PRIMITIVE

void FArchiveJson::Serialize(std::string_view Name, uint8& Value)
{
    uint32 Temp = Value;
    Serialize(Name, Temp);
    if (IsLoading()) Value = static_cast<uint8>(Temp);
}


// ---------------------------------------------------------
// Engine Core
// ---------------------------------------------------------
void FArchiveJson::Serialize(std::string_view Name, FString& Value)
{
    rapidjson::Value* Current = NodeStack.back(); 
    if (!Current) 
        return;

    if (IsSaving()) 
    {
        rapidjson::Value JsonVal(Value.c_str(), static_cast<uint32>(Value.size()), *Allocator);
        AddChildNode(Current, Name, JsonVal);
    }
    else 
    {
        rapidjson::Value* Child = GetChildNode(Current, Name);
        if (Child && Child->IsString()) 
            Value = Child->GetString();
    }
}

void FArchiveJson::Serialize(std::string_view Name, FGuid& Value)
{
    FString GuidStr = Value.ToString();
    Serialize(Name, GuidStr);
    if (IsLoading()) 
        Value.Parse(GuidStr);
}


// ---------------------------------------------------------
// Math Types
// ---------------------------------------------------------
void FArchiveJson::Serialize(std::string_view Name, FVector2D& Value)
{
    rapidjson::Value* Current = NodeStack.back(); 
    if (!Current) 
        return;

    if (IsSaving())
    {
        rapidjson::Value ArrayVal(rapidjson::kArrayType);
        ArrayVal.PushBack(Value.x, *Allocator)
            .PushBack(Value.y, *Allocator);
        AddChildNode(Current, Name, ArrayVal);
    }
    else 
    {
        rapidjson::Value* Child = GetChildNode(Current, Name);
        if (Child && Child->IsArray() && Child->Size() >= 2) {
            Value.x = (*Child)[0].GetFloat();
            Value.y = (*Child)[1].GetFloat();
        }
    }
}

void FArchiveJson::Serialize(std::string_view Name, FVector3& Value)
{
    rapidjson::Value* Current = NodeStack.back(); 
    if (!Current) 
        return;

    if (IsSaving()) {
        rapidjson::Value ArrayVal(rapidjson::kArrayType);
        ArrayVal.PushBack(Value.x, *Allocator)
            .PushBack(Value.y, *Allocator)
            .PushBack(Value.z, *Allocator);
        AddChildNode(Current, Name, ArrayVal);
    }
    else 
    {
        rapidjson::Value* Child = GetChildNode(Current, Name);
        if (Child && Child->IsArray() && Child->Size() >= 3) {
            Value.x = (*Child)[0].GetFloat();
            Value.y = (*Child)[1].GetFloat();
            Value.z = (*Child)[2].GetFloat();
        }
    }
}

void FArchiveJson::Serialize(std::string_view Name, FVector4& Value)
{
    rapidjson::Value* Current = NodeStack.back(); if (!Current) return;

    if (IsSaving()) 
    {
        rapidjson::Value ArrayVal(rapidjson::kArrayType);
        ArrayVal.PushBack(Value.x, *Allocator)
            .PushBack(Value.y, *Allocator)
            .PushBack(Value.z, *Allocator)
            .PushBack(Value.w, *Allocator);
        AddChildNode(Current, Name, ArrayVal);
    }
    else 
    {
        rapidjson::Value* Child = GetChildNode(Current, Name);
        if (Child && Child->IsArray() && Child->Size() >= 4) {
            Value.x = (*Child)[0].GetFloat();
            Value.y = (*Child)[1].GetFloat();
            Value.z = (*Child)[2].GetFloat();
            Value.w = (*Child)[3].GetFloat();
        }
    }
}

void FArchiveJson::Serialize(std::string_view Name, FQuat& Value)
{
    FVector4 Temp(Value.x, Value.y, Value.z, Value.w);

    // FVector4  재사용
    Serialize(Name, Temp);

    if (IsLoading())
    {
        Value.x = Temp.x;
        Value.y = Temp.y;
        Value.z = Temp.z;
        Value.w = Temp.w;
    }
}

void FArchiveJson::Serialize(std::string_view Name, FMatrix& Value)
{
    rapidjson::Value* Current = NodeStack.back(); 
    if (!Current) 
        return;

    if (IsSaving()) 
    {
        rapidjson::Value ArrayVal(rapidjson::kArrayType);

        for (int i = 0; i < 4; ++i) 
        {
            for (int j = 0; j < 4; ++j) 
            {
                ArrayVal.PushBack(Value.m[i][j], *Allocator);
            }
        }
        AddChildNode(Current, Name, ArrayVal);
    }
    else 
    {
        rapidjson::Value* Child = GetChildNode(Current, Name);
        if (Child && Child->IsArray() && Child->Size() >= 16) 
        {
            int index = 0;
            for (int i = 0; i < 4; ++i) 
            {
                for (int j = 0; j < 4; ++j) 
                {
                    Value.m[i][j] = (*Child)[index++].GetFloat();
                }
            }
        }
    }
}


// ---------------------------------------------------------
// Scope
// ---------------------------------------------------------
void FArchiveJson::BeginObjectScope(std::string_view Name)
{
    rapidjson::Value* Current = NodeStack.back();
    if (!Current) 
    { 
        NodeStack.push_back(nullptr); 
        return; 
    }

    if (IsSaving())
    {
        rapidjson::Value NewObj(rapidjson::kObjectType);
        AddChildNode(Current, Name, NewObj);

        if (Current->IsObject()) 
        {
            std::string KeyStr(Name);
            NodeStack.push_back(&(*Current)[KeyStr.c_str()]);
        }
        else if (Current->IsArray()) 
        {
            NodeStack.push_back(&(*Current)[Current->Size() - 1]);
        }
    }
    else if (IsLoading()) 
    {
        rapidjson::Value* Child = GetChildNode(Current, Name);
        if (Child && Child->IsObject()) 
            NodeStack.push_back(Child);
        else 
            NodeStack.push_back(nullptr);
    }
}

void FArchiveJson::BeginArrayScope(std::string_view Name, size_t& ArraySize)
{
    rapidjson::Value* Current = NodeStack.back();
    if (!Current) 
    { 
        NodeStack.push_back(nullptr); return;
    }

    if (IsSaving()) 
    {
        rapidjson::Value NewArray(rapidjson::kArrayType);
        AddChildNode(Current, Name, NewArray);

        if (Current->IsObject()) 
        {
            std::string KeyStr(Name);
            NodeStack.push_back(&(*Current)[KeyStr.c_str()]);
        }
        else if (Current->IsArray()) 
        {
            NodeStack.push_back(&(*Current)[Current->Size() - 1]);
        }
    }
    else if (IsLoading()) 
    {
        rapidjson::Value* Child = GetChildNode(Current, Name);
        if (Child && Child->IsArray()) 
        {
            ArraySize = Child->Size();
            NodeStack.push_back(Child);
        }
        else 
        {
            ArraySize = 0;
            NodeStack.push_back(nullptr);
        }
    }
}