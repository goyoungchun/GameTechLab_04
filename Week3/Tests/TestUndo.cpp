#include "PCH.h"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"


#include "../Core/Base/UObject.h"
#include "../Core/Base/UndoSystem.h"
#include "../UObjectSystem.h"


TEST_CASE("Undo Test") 
{
    /*
    std::unique_ptr<UObject> TestObject = std::make_unique<UObject>();
    FObjectHandle Handle = UObjectSystem::Register(TestObject.get());

 
    CHECK(TestObject->Data == 3);

    UndoSystem::Undo();
    CHECK(TestObject->Data == 3);
    UndoSystem::Redo();
    CHECK(TestObject->Data == 3);


    UndoSystem::BeginTransaction("Test");
    UndoSystem::Modify(TestObject.get());
    CHECK(TestObject->Data == 3);
    
    TestObject->Data++;
    UndoSystem::EndTransaction();


    CHECK(TestObject->Data == 4);
    
    UndoSystem::Redo();
    CHECK(TestObject->Data == 4);

    UndoSystem::Undo();
    CHECK(TestObject->Data == 3);

    UndoSystem::Undo();
    CHECK(TestObject->Data == 3);

    UndoSystem::Redo();
    CHECK(TestObject->Data == 4);

    UObjectSystem::Unregister(TestObject.get(), Handle);
    */
}