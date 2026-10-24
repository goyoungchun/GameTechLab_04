#pragma once
#include "Delegate.h"

#define DECLARE_DELEGATE(DelegateName) PRIVATE_DECLARE_DELEGATE(DelegateName, void())

#define DECLARE_MULTICAST_DELEGATE(DelegateName) PRIVATE_DECLARE_MULTICAST_DELEGATE(DelegateName, void())

#define DECLARE_DELEGATE_RetVal(RetValueType, DelegateName) PRIVATE_DECLARE_DELEGATE(DelegateName, RetValueType())

// Multiple-parameter versions of above delegate types:
#define DECLARE_DELEGATE_OneParam(DelegateName, Param1Type) PRIVATE_DECLARE_DELEGATE(DelegateName, void(Param1Type))
#define DECLARE_MULTICAST_DELEGATE_OneParam(DelegateName, Param1Type) PRIVATE_DECLARE_MULTICAST_DELEGATE(DelegateName, void(Param1Type))
#define DECLARE_DELEGATE_RetVal_OneParam(ReturnValueType, DelegateName, Param1Type) PRIVATE_DECLARE_DELEGATE(DelegateName, ReturnValueType(Param1Type))

#define DECLARE_DELEGATE_TwoParams(DelegateName, Param1Type, Param2Type) PRIVATE_DECLARE_DELEGATE(DelegateName, void(Param1Type, Param2Type))
#define DECLARE_MULTICAST_DELEGATE_TwoParams(DelegateName, Param1Type, Param2Type) PRIVATE_DECLARE_MULTICAST_DELEGATE(DelegateName, void(Param1Type, Param2Type))
#define DECLARE_DELEGATE_RetVal_TwoParams(ReturnValueType, DelegateName, Param1Type, Param2Type) PRIVATE_DECLARE_DELEGATE(DelegateName, ReturnValueType(Param1Type, Param2Type))

#define DECLARE_DELEGATE_ThreeParams(DelegateName, Param1Type, Param2Type, Param3Type) PRIVATE_DECLARE_DELEGATE(DelegateName, void(Param1Type, Param2Type, Param3Type))
#define DECLARE_MULTICAST_DELEGATE_ThreeParams(DelegateName, Param1Type, Param2Type, Param3Type) PRIVATE_DECLARE_MULTICAST_DELEGATE(DelegateName, void(Param1Type, Param2Type, Param3Type))
#define DECLARE_DELEGATE_RetVal_ThreeParams(ReturnValueType, DelegateName, Param1Type, Param2Type, Param3Type) PRIVATE_DECLARE_DELEGATE(DelegateName, ReturnValueType(Param1Type, Param2Type, Param3Type))

#define DECLARE_DELEGATE_FourParams(DelegateName, Param1Type, Param2Type, Param3Type, Param4Type) PRIVATE_DECLARE_DELEGATE( DelegateName, void(Param1Type, Param2Type, Param3Type, Param4Type))
#define DECLARE_MULTICAST_DELEGATE_FourParams(DelegateName, Param1Type, Param2Type, Param3Type, Param4Type) PRIVATE_DECLARE_MULTICAST_DELEGATE(DelegateName, void(Param1Type, Param2Type, Param3Type, Param4Type))
#define DECLARE_DELEGATE_RetVal_FourParams(ReturnValueType, DelegateName, Param1Type, Param2Type, Param3Type, Param4Type) PRIVATE_DECLARE_DELEGATE(DelegateName, ReturnValueType(Param1Type, Param2Type, Param3Type, Param4Type))

#define DECLARE_DELEGATE_FiveParams(DelegateName, Param1Type, Param2Type, Param3Type, Param4Type, Param5Type) PRIVATE_DECLARE_DELEGATE(DelegateName, void(Param1Type, Param2Type, Param3Type, Param4Type, Param5Type))
#define DECLARE_MULTICAST_DELEGATE_FiveParams(DelegateName, Param1Type, Param2Type, Param3Type, Param4Type, Param5Type) PRIVATE_DECLARE_MULTICAST_DELEGATE(DelegateName, void(Param1Type, Param2Type, Param3Type, Param4Type, Param5Type))
#define DECLARE_DELEGATE_RetVal_FiveParams(ReturnValueType, DelegateName, Param1Type, Param2Type, Param3Type, Param4Type, Param5Type) PRIVATE_DECLARE_DELEGATE(DelegateName, ReturnValueType(Param1Type, Param2Type, Param3Type, Param4Type, Param5Type))

#define DECLARE_DELEGATE_SixParams(DelegateName, Param1Type, Param2Type, Param3Type, Param4Type, Param5Type, Param6Type) PRIVATE_DECLARE_DELEGATE(DelegateName, void(Param1Type, Param2Type, Param3Type, Param4Type, Param5Type, Param6Type))
#define DECLARE_MULTICAST_DELEGATE_SixParams(DelegateName, Param1Type, Param2Type, Param3Type, Param4Type, Param5Type, Param6Type) PRIVATE_DECLARE_MULTICAST_DELEGATE(DelegateName, void(Param1Type, Param2Type, Param3Type, Param4Type, Param5Type, Param6Type))
#define DECLARE_DELEGATE_RetVal_SixParams(ReturnValueType, DelegateName, Param1Type, Param2Type, Param3Type, Param4Type, Param5Type, Param6Type) PRIVATE_DECLARE_DELEGATE(DelegateName, ReturnValueType(Param1Type, Param2Type, Param3Type, Param4Type, Param5Type, Param6Type))

#define DECLARE_DELEGATE_SevenParams(DelegateName, Param1Type, Param2Type, Param3Type, Param4Type, Param5Type, Param6Type, Param7Type) PRIVATE_DECLARE_DELEGATE(DelegateName, void(Param1Type, Param2Type, Param3Type, Param4Type, Param5Type, Param6Type, Param7Type))
#define DECLARE_MULTICAST_DELEGATE_SevenParams(DelegateName, Param1Type, Param2Type, Param3Type, Param4Type, Param5Type, Param6Type, Param7Type) PRIVATE_DECLARE_MULTICAST_DELEGATE(DelegateName, void(Param1Type, Param2Type, Param3Type, Param4Type, Param5Type, Param6Type, Param7Type))
#define DECLARE_DELEGATE_RetVal_SevenParams(ReturnValueType, DelegateName, Param1Type, Param2Type, Param3Type, Param4Type, Param5Type, Param6Type, Param7Type) PRIVATE_DECLARE_DELEGATE(DelegateName, ReturnValueType(Param1Type, Param2Type, Param3Type, Param4Type, Param5Type, Param6Type, Param7Type))

#define DECLARE_DELEGATE_EightParams(DelegateName, Param1Type, Param2Type, Param3Type, Param4Type, Param5Type, Param6Type, Param7Type, Param8Type) PRIVATE_DECLARE_DELEGATE(DelegateName, void(Param1Type, Param2Type, Param3Type, Param4Type, Param5Type, Param6Type, Param7Type, Param8Type))
#define DECLARE_MULTICAST_DELEGATE_EightParams(DelegateName, Param1Type, Param2Type, Param3Type, Param4Type, Param5Type, Param6Type, Param7Type, Param8Type) PRIVATE_DECLARE_MULTICAST_DELEGATE(DelegateName, void(Param1Type, Param2Type, Param3Type, Param4Type, Param5Type, Param6Type, Param7Type, Param8Type))
#define DECLARE_DELEGATE_RetVal_EightParams(ReturnValueType, DelegateName, Param1Type, Param2Type, Param3Type, Param4Type, Param5Type, Param6Type, Param7Type, Param8Type) PRIVATE_DECLARE_DELEGATE(DelegateName, ReturnValueType(Param1Type, Param2Type, Param3Type, Param4Type, Param5Type, Param6Type, Param7Type, Param8Type))

#define DECLARE_DELEGATE_NineParams(DelegateName, Param1Type, Param2Type, Param3Type, Param4Type, Param5Type, Param6Type, Param7Type, Param8Type, Param9Type) PRIVATE_DECLARE_DELEGATE(DelegateName, void(Param1Type, Param2Type, Param3Type, Param4Type, Param5Type, Param6Type, Param7Type, Param8Type, Param9Type))
#define DECLARE_MULTICAST_DELEGATE_NineParams(DelegateName, Param1Type, Param2Type, Param3Type, Param4Type, Param5Type, Param6Type, Param7Type, Param8Type, Param9Type) PRIVATE_DECLARE_MULTICAST_DELEGATE(DelegateName, void(Param1Type, Param2Type, Param3Type, Param4Type, Param5Type, Param6Type, Param7Type, Param8Type, Param9Type))
#define DECLARE_DELEGATE_RetVal_NineParams(ReturnValueType, DelegateName, Param1Type, Param2Type, Param3Type, Param4Type, Param5Type, Param6Type, Param7Type, Param8Type, Param9Type) PRIVATE_DECLARE_DELEGATE( DelegateName, ReturnValueType(Param1Type, Param2Type, Param3Type, Param4Type, Param5Type, Param6Type, Param7Type, Param8Type, Param9Type))
