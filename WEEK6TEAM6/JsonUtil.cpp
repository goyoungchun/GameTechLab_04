#include "JsonUtil.h"
#include "Json/json.hpp"

namespace JsonUtils
{
	json::JSON ToJson(const FString& String)
	{
		return json::JSON(String.c_str());
	}

	json::JSON ToJson(const FVector4& Vector)
	{
		json::JSON vectorJson = json::JSON::Make(json::JSON::Class::Array);
		vectorJson[0] = Vector.x;
		vectorJson[1] = Vector.y;
		vectorJson[2] = Vector.z;
		vectorJson[3] = Vector.w;
		return vectorJson;
	}

	json::JSON ToJson(const FVector2& Vector)
	{
		json::JSON vectorJson = json::JSON::Make(json::JSON::Class::Array);
		vectorJson[0] = Vector.X;
		vectorJson[1] = Vector.Y;
		return vectorJson;
	}

	json::JSON ToJson(const FVector& Vector)
	{
		json::JSON vectorJson = json::JSON::Make(json::JSON::Class::Array);
		vectorJson[0] = Vector.x;
		vectorJson[1] = Vector.y;
		vectorJson[2] = Vector.z;
		return vectorJson;
	}

	json::JSON ToJson(const FRotator& Rotator)
	{
		json::JSON rotatorJson = json::JSON::Make(json::JSON::Class::Array);
		rotatorJson[0] = Rotator.Pitch;
		rotatorJson[1] = Rotator.Yaw;
		rotatorJson[2] = Rotator.Roll;
		return rotatorJson;
	}

	json::JSON ToJson(const EPrimitive& Primitive)
	{
		switch (Primitive)
		{
		case EPrimitive::EP_Sphere:
			return json::JSON("Sphere");
		case EPrimitive::EP_Cube:
			return json::JSON("Cube");
		case EPrimitive::EP_Triangle:
			return json::JSON("Triangle");
		case EPrimitive::EP_GizmoArrow:
			return json::JSON("GizmoArrow");
		case EPrimitive::EP_Circle:
			return json::JSON("Circle");
		case EPrimitive::EP_Plane:
			return json::JSON("Plane");
		default:
			throw std::runtime_error("Unknown EPrimitive value");
		}
	}

	json::JSON ToJson(const FGuid& Guid)
	{
		json::JSON GuidJson = json::JSON::Make(json::JSON::Class::Object);
		GuidJson["A"] = Guid.A;
		GuidJson["B"] = Guid.B;
		GuidJson["C"] = Guid.C;
		GuidJson["D"] = Guid.D;
		return GuidJson;
	}
	json::JSON ToJson(const FLinearColor& LinearColor)
	{
		json::JSON FLinearColorJson = json::JSON::Make(json::JSON::Class::Array);
		FLinearColorJson[0] = LinearColor.R;
		FLinearColorJson[1] = LinearColor.G;
		FLinearColorJson[2] = LinearColor.B;
		FLinearColorJson[3] = LinearColor.A;
		return FLinearColorJson;
	}
}