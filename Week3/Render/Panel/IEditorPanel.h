#pragma once

class IEditorPanel
{
public:
    virtual ~IEditorPanel() = default;

    virtual void DrawPanel() = 0;

    bool IsVisible() const 
    {
        return bVisible;
    }

    void SetVisible(bool bInVisible) 
    {
        bVisible = bInVisible;
    }

protected:
    bool bVisible = true;
};