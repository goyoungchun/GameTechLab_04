#include "PCH.h"
#include "FAssetHandle.h"
#include "Serialize/FArchive.h"

void FAssetHandle::Serialize(FArchive& Archive)
{
	Archive.Serialize("ID", ID);
	Archive.Serialize("Generation", Generation);
}
