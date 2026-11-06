#pragma once
#include "BaseComponent.h"
#include "AIProtoManager.h"
#include "AICommands.h"

class BaseAICommand;

/*
	Some data copied from Physic component for use by AI.
*/
struct SAIData
{
	float m_x;
	float m_y;
	float m_angle;
};

/*
	Component that is used to control object's behaviour, either
	for Player or AI
*/
class ControlComponent : public SubComponent<ControlComponent, GameObject>
{
public:
	enum EType
	{
		TYPE_PLAYER,
		TYPE_AI,
	};

	ControlComponent() = default;

	ControlComponent( const ControlCompID & id )
		: SubComponent<ControlComponent, GameObject>( id )
	{}

	  // Type
	EType m_type = TYPE_PLAYER;
	  // Mask of control flags
	int m_controls = 0;
	  // See EDirection
	int m_direction = 1;

	  // AI Lua function to be run at the each frame
	std::string m_function;

	  // State that is used to run m_function. Actually, just a Lua state hidden begind interface
	std::unique_ptr<IAIState> m_pState;
	  // Current trigger. Until it fires, m_function would not be run
	std::unique_ptr<IAITrigger> m_pCurrentTrigger;
      // Data from other components for AI to use
	SAIData m_aiData{};

	void SetTrigger( IAITrigger *pTrigger )
	{
		m_pCurrentTrigger.reset(pTrigger);
	}
};
