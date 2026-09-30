#pragma once
#include "ViewportTypes.h"
#include "FEditorViewportClient.h"

struct FSceneView;

class FViewport
{
public:
	FViewport(uint8 id, EViewportType type, FViewportSharedSettings& sharedSettings) :
		Id(id),Type(type),Client(std::make_unique<FEditorViewportClient>(sharedSettings, type))
	{
	}
	~FViewport() = default;

	FSceneView buildSceneView() const;

	void setType(EViewportType type) { Type = type; Client->setViewportSettings(type); }
	FViewportRenderSettings& getRenderSettings() {return renderSettings;}
	const FViewportRenderSettings& getRenderSettings() const{return renderSettings;}

	uint8 getId() const;
	EViewportType getType() const;

	FEditorViewportClient& getClient();
	const FEditorViewportClient& getClient() const;

	FViewportWindowState& getWindowState();
	const FViewportWindowState& getWindowState() const;
private:
	friend class FEditorViewportManager;

	void setId(uint8 id);
	uint8 Id = invalidViewportId;
	EViewportType Type = EViewportType::Perspective;
	std::unique_ptr<FEditorViewportClient> Client;
	FViewportWindowState windowState;
	FViewportRenderSettings renderSettings;
};
