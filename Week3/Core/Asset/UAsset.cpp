#include "PCH.h"
#include "UAsset.h"

void UAsset::Serialize(FArchive& Ar) {
	UObject::Serialize(Ar);
	Ar.Serialize("AssetName", AssetName);

	FString PathStr = FString{ AssetMetaDataPath.string() };
	Ar.Serialize("AssetMetaDataPath", PathStr);
}
