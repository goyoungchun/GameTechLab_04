#include "Viewport.h"
#include "FEditorViewportClient.h"
#include "../Rendering/SceneView.h"

uint8 FViewport::getId() const
{
	return Id;
}
EViewportType FViewport::getType() const
{
	return Type;
}
FEditorViewportClient& FViewport::getClient()
{
	return *Client;
}
const FEditorViewportClient& FViewport::getClient() const
{
	return *Client;
}
FViewportWindowState& FViewport::getWindowState()
{
	return windowState;
}
const FViewportWindowState& FViewport::getWindowState() const
{
	return windowState;
}
void FViewport::setId(uint8 id)
{
	Id = id;
}

FSceneView FViewport::buildSceneView() const
{
	const FRect& imageRect = windowState.imageRect;
	const FViewRect renderRect{ imageRect.Left, imageRect.Top, imageRect.getWidth(), imageRect.getHeight() };

	FSceneView view = makeSceneView(Client->GetCamera(), renderRect, Client->getProjectionRatio());
	view.viewMode = renderSettings.ViewMode;
	view.showFlags = renderSettings.ShowFlags;
	return view;
}
