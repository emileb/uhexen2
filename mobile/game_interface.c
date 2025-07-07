#include "quakedef.h"

#include "game_interface.h"

#include "keys.h"

#include "SDL.h"
#include "SDL_keycode.h"

#include "SmartToggle.h"
#include "CStringFifo.h"

static float look_pitch_mouse,look_pitch_abs,look_pitch_joy;
static float look_yaw_mouse,look_yaw_joy;;
static CStringFIFO m_CmdFifo;


int main_android (int c, const char **v);
void PortableInit(int argc,const char ** argv)
{
	LOGI("PortableInit");
    cstr_fifo_init(&m_CmdFifo);

	setenv("TIMIDITY_CFG", "midi/timidity.cfg",1);

    main_android( argc, argv );
}


extern int SDL_SendKeyboardKey(Uint8 state, SDL_Scancode scancode);

int PortableKeyEvent(int state, int code ,int unitcode)
{
	//LOGI("PortableKeyEvent %d %d",state,code);

    if (state)
        SDL_SendKeyboardKey(SDL_PRESSED, (SDL_Scancode)code);
    else
        SDL_SendKeyboardKey(SDL_RELEASED, (SDL_Scancode) code);

	return 0;
}

extern kbutton_t	in_mlook, in_klook;
extern kbutton_t	in_left, in_right, in_forward, in_back;
extern kbutton_t	in_lookup, in_lookdown, in_moveleft, in_moveright;
extern kbutton_t	in_strafe, in_speed, in_use, in_jump, in_attack;
extern kbutton_t	in_up, in_down, in_crouch;
extern kbutton_t	in_infoplaque;

void KeyUpPort (kbutton_t *b)
{
	b->state = 4; 		// impulse up
}
void KeyDownPort (kbutton_t *b)
{
	b->state |= 1 + 2; // down + impulse down
}

static int KeyIsDown (kbutton_t *b)
{
    return b->state & 0x1;
}

static int scoresShown = 0;
void PortableAction(int state, int action)
{
	LOGI("PortableAction %d %d",state, action);

	if ((action >= PORT_ACT_CUSTOM_0) && (action <= PORT_ACT_CUSTOM_17))
    {
        PortableKeyEvent(state, SDL_SCANCODE_H + action - PORT_ACT_CUSTOM_0, 0);
    }

	if (( PortableGetScreenMode() == TS_MENU ) || ( PortableGetScreenMode() == TS_BLANK )  || ( PortableGetScreenMode() == TS_Y_N ))
    {
        if (action >= PORT_ACT_MENU_UP && action <= PORT_ACT_MENU_ABORT)
        {
            int sdl_code [] = { SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT,
                    SDL_SCANCODE_RIGHT, SDL_SCANCODE_RETURN, SDL_SCANCODE_ESCAPE, SDL_SCANCODE_Y, SDL_SCANCODE_N };

            PortableKeyEvent(state, sdl_code[action-PORT_ACT_MENU_UP], 0);

            return;
        }
        else if( action == PORT_ACT_CONSOLE )
        {
            if (state)
               PortableCommand("toggleconsole");
        }
    }
    else
    {
        switch (action)
        {
       case PORT_ACT_LEFT:
            (state)?KeyDownPort(&in_left):KeyUpPort(&in_left);
            break;
        case PORT_ACT_RIGHT:
            (state)?KeyDownPort(&in_right):KeyUpPort(&in_right);
            break;
        case PORT_ACT_FWD:
            (state)?KeyDownPort(&in_forward):KeyUpPort(&in_forward);
            break;
        case PORT_ACT_BACK:
            (state)?KeyDownPort(&in_back):KeyUpPort(&in_back);
            break;
        case PORT_ACT_LOOK_UP:
            (state)?KeyDownPort(&in_lookup):KeyUpPort(&in_lookup);
            break;
        case PORT_ACT_LOOK_DOWN:
            (state)?KeyDownPort(&in_lookdown):KeyUpPort(&in_lookdown);
            break;
        case PORT_ACT_MOVE_LEFT:
            (state)?KeyDownPort(&in_moveleft):KeyUpPort(&in_moveleft);
            break;
        case PORT_ACT_MOVE_RIGHT:
            (state)?KeyDownPort(&in_moveright):KeyUpPort(&in_moveright);
            break;
        case PORT_ACT_STRAFE:
            (state)?KeyDownPort(&in_strafe):KeyUpPort(&in_strafe);
            break;
        case PORT_ACT_SPEED:
            (state)?KeyDownPort(&in_speed):KeyUpPort(&in_speed);
            break;
        case PORT_ACT_USE:
            (state)?KeyDownPort(&in_use):KeyUpPort(&in_use);
            break;
        case PORT_ACT_JUMP:
            (state)?KeyDownPort(&in_jump):KeyUpPort(&in_jump);
            break;
        case PORT_ACT_ATTACK:
            (state)?KeyDownPort(&in_attack):KeyUpPort(&in_attack);
            break;
        case PORT_ACT_UP:
            (state)?KeyDownPort(&in_up):KeyUpPort(&in_up);
            break;
        case PORT_ACT_DOWN:
            (state)?KeyDownPort(&in_crouch):KeyUpPort(&in_crouch);
            break;
        case PORT_ACT_TOGGLE_CROUCH:
            {
                static SmartToggle_t smartToggle;
                int activate = SmartToggleAction( &smartToggle, state, KeyIsDown(&in_crouch));
                (activate)?KeyDownPort(&in_crouch):KeyUpPort(&in_crouch);
                break;
            }
        case PORT_ACT_WEAP1:
            if ( state )
                PortableCommand("impulse 1\n");
            break;
        case PORT_ACT_WEAP2:
            if ( state )
                PortableCommand("impulse 2\n");
            break;
        case PORT_ACT_WEAP3:
            if ( state )
                PortableCommand("impulse 3\n");
            break;
        case PORT_ACT_WEAP4:
            if ( state )
                PortableCommand("impulse 4\n");
            break;
        case PORT_ACT_WEAP5:
            if ( state )
                PortableCommand("impulse 5\n");
            break;
        case PORT_ACT_WEAP6:
            if ( state )
                PortableCommand("impulse 6\n");
            break;
        case PORT_ACT_WEAP7:
            if ( state )
                PortableCommand("impulse 7\n");
            break;
        case PORT_ACT_WEAP8:
            if ( state )
                PortableCommand("impulse 8\n");
            break;
        case PORT_ACT_NEXT_WEP:
            if (state)
                PortableCommand("impulse 10\n");
            break;
        case PORT_ACT_PREV_WEP:
            if (state)
                PortableCommand("impulse 12\n");
            break;
        case PORT_MALICE_USE:
            if (state)
                PortableCommand("impulse 13\n");
            break;
        // HEXEN 2 ----------------------------
        case PORT_ACT_CROUCH:
            if(state)
                PortableCommand("+crouch");
            else
                PortableCommand("-crouch");
            break;
        case PORT_ACT_INVUSE:
            if (state)
                PortableCommand("invuse\n");
            break;
        case PORT_ACT_INVDROP:
            if (state)
                PortableCommand("impulse 44\n");
            break;
        case PORT_ACT_INVPREV:
            if (state)
                PortableCommand("invleft\n");
            break;
        case PORT_ACT_INVNEXT:
            if (state)
                PortableCommand("invright\n");
            break;
 	    case PORT_ACT_HELPCOMP:
 		    if (state)
 			    PortableCommand("+showinfo\n");
            else
                PortableCommand("-showinfo\n");
 		    break;
            // -------------------------------------
		case PORT_ACT_QUICKSAVE:
			if(state) {
                PortableCommand("save quick\n");
			}
			break;
		case PORT_ACT_QUICKLOAD:
			if(state) {
                PortableCommand("load quick\n");
			}
			break;
        case PORT_ACT_CONSOLE:
            if (state)
                PortableCommand("toggleconsole");
            break;
        case PORT_ACT_MP_SCORES:
            if(state)
            {
                if (scoresShown)
                    PortableCommand("-showscores\n");
                else
                    PortableCommand("+showscores\n");
                scoresShown = !scoresShown;
            }
            break;
        }
	}
}

void PortableCommand(const char * cmd)
{
    cstr_fifo_push(&m_CmdFifo, cmd);
}


extern int key_dest;
extern int android_Y_N;
touchscreemode_t PortableGetScreenMode()
{
    if( android_Y_N )
    {
        return TS_Y_N;
    }
    else if(Key_GetDest() != key_game || cls.state == ca_disconnected )
    {
        return TS_MENU;
    }
    else
    {
        return TS_GAME;
    }
}

void PortableBackButton()
{
    LOGI("Back button");
    PortableKeyEvent(1, SDL_SCANCODE_ESCAPE,0 );
    PortableKeyEvent(0, SDL_SCANCODE_ESCAPE, 0);
}

// =================== FORWARD and SIDE MOVMENT ==============

float forwardmove, sidemove; //Joystick mode

void PortableMoveFwd(float fwd)
{
	if (fwd > 1)
		fwd = 1;
	else if (fwd < -1)
		fwd = -1;

	forwardmove = fwd;
}

void PortableMoveSide(float strafe)
{
	if (strafe > 1)
		strafe = 1;
	else if (strafe < -1)
		strafe = -1;

	sidemove = strafe;
}

void PortableMove(float fwd, float strafe)
{
	PortableMoveFwd(fwd);
	PortableMoveSide(strafe);
}

//======================================================================

//Look up and down

void PortableLookPitch(int mode, float pitch)
{
	switch(mode)
	{
	case LOOK_MODE_MOUSE:
		look_pitch_mouse += pitch;
		break;
	case LOOK_MODE_JOYSTICK:
		look_pitch_joy = pitch;
		break;
	}
}

//left right

void PortableLookYaw(int mode, float yaw)
{
	switch(mode)
	{
	case LOOK_MODE_MOUSE:
		look_yaw_mouse += yaw;
		break;
	case LOOK_MODE_JOYSTICK:
		look_yaw_joy = yaw;
		break;
	}
}

bool PortableSetAlwaysRun(bool run)
{
    if(run)
    {
        PortableCommand("cl_forwardspeed 400\n");
        PortableCommand("cl_backspeed 400\n");
    }
    else
    {
        PortableCommand("cl_forwardspeed 200\n");
        PortableCommand("cl_backspeed 200\n");
    }
    return false;
}

/////////////////////
// Movement handling
////

void IN_Move_Android (usercmd_t *cmd)
{
    char *consoleCmd;
    while((consoleCmd = cstr_fifo_pop(&m_CmdFifo)))
    {
        Cmd_ExecuteString(consoleCmd, src_command);
        free(consoleCmd);
    }

    int blockGamepad( void );
    int blockMove = blockGamepad() & ANALOGUE_AXIS_FWD;
    int blockLook = blockGamepad() & ANALOGUE_AXIS_PITCH;

    int fwdSpeed =  400;
    int sideSpeed = 400;

    int isPlayerRunning();
    if(!isPlayerRunning())
    {
        fwdSpeed = fwdSpeed / 2;
        sideSpeed = sideSpeed /2;
    }

    if( !blockMove )
    {
        cmd->forwardmove += forwardmove * fwdSpeed;
        cmd->sidemove  += sidemove * sideSpeed;
    }

    if( !blockLook )
    {
        cl.viewangles[PITCH] += -look_pitch_mouse * 300;
        look_pitch_mouse = 0;
        cl.viewangles[PITCH] += look_pitch_joy * 6 * (host_frametime * 1000.f / 16.f); // Presume was scaled at 60FPS;;

        cl.viewangles[YAW] += look_yaw_mouse * 400;
        look_yaw_mouse = 0;
        cl.viewangles[YAW] += look_yaw_joy * 6 * (host_frametime * 1000.f / 16.f); // Presume was scaled at 60FPS;;
    }

	V_StopPitchDrift();
}

