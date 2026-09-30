#pragma once

enum EKeyStatus {
	SPACE,
	ENTER,
	LEFT,
	RIGHT,
	UP,
	DOWN,
	KEY_COUNT
};

class InputManager {
public:
	InputManager() = default;
	~InputManager() = default;
	InputManager(const InputManager&) = delete;
	InputManager& operator = (const InputManager&) = delete;
	static InputManager* GetInstance() 
	{
		if (!_instance)
			_instance = new InputManager();
		return _instance;
	};
private:
	static InputManager* _instance;
	bool bKeyStatus[KEY_COUNT] = {};

public:
	bool* GetSTATUS() { return bKeyStatus; }
	void OnKeyDown(EKeyStatus key) { bKeyStatus[key] = true; }
	void OnKeyUP(EKeyStatus key) { bKeyStatus[key] = false; }
};