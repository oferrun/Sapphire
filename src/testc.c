#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "imgui/cimgui.h"
#include "imgui/cimguizmo.h"

#include <memory.h>
#include "RenderDevice.h"
#include "SwapChain.h"
#include "DeviceContext.h"
#include "Shader.h"

#include "GraphicsUtilities.h"
#include "TextureUtilities.h"



#include "core/sapphire_types.h"
#include "core/sapphire_math.h"
#include "core/json.h"
#include "core/config.h"
#include "core/allocator.h"
#include "core/temp_allocator.h"
#include "core/os.h"
#include "core/murmurhash64a.h"
#include "core/array.h"
#include "core/hash.h"
#include "core/camera.h"
#include "core/sprintf.h"
#include "sapphire_renderer.h"
#include "scene.h"

void sapphire_render(IDeviceContext* pContext);
void sapphire_init(IRenderDevice* p_device, ISwapChain* p_swap_chain);
void sapphire_destroy();
void sapphire_window_resize(IRenderDevice* pDevice, ISwapChain* pSwapChain, uint32_t width, uint32_t height);
void sapphire_update(double curr_time, double elapsed_time);







////
bool read_file_stream(const char* file, sp_temp_allocator_i* ta, uint8_t** p_stream, uint64_t* out_size);
bool read_grimrock_model_from_stream(const uint8_t* p_stream, uint64_t size, sapphire_mesh_gpu_load_t* p_mesh_load);




/////
typedef struct viewer_t
{
    //tm_vec3_t damped_translation;
    //float translation_speed;
    sp_transform_t camera_transform;
    sp_camera_t camera;
    sp_mat4x4_t view_projection;
    //sp_mat4x4_t last_view_projection;

} viewer_t;

static viewer_t g_viewer;
static rendering_context_t* g_rendering_context_o;






// called when window is resized
// recreates the render target for the new window size
void sapphire_window_resize(IRenderDevice* pDevice, ISwapChain* pSwapChain, uint32_t width, uint32_t height)
{
    renderer_window_resize(pDevice, pSwapChain, width, height);         
}



// Common input items for keyboards -- the first `0xff` entries in this list correspond to Windows
// virtual key codes, subsequent items are added to extend with keys available on other systems.
enum sp_input_code_item {
    // Standard windows keys.

    SP_INPUT_KEYBOARD_ITEM_NONE,
    SP_INPUT_KEYBOARD_ITEM_LBUTTON = 0x01,
    SP_INPUT_KEYBOARD_ITEM_RBUTTON = 0x02,
    SP_INPUT_KEYBOARD_ITEM_CANCEL = 0x03,
    SP_INPUT_KEYBOARD_ITEM_MBUTTON = 0x04,
    SP_INPUT_KEYBOARD_ITEM_XBUTTON1 = 0x05,
    SP_INPUT_KEYBOARD_ITEM_XBUTTON2 = 0x06,
    SP_INPUT_KEYBOARD_ITEM_BACKSPACE = 0x08, // VK_BACK
    SP_INPUT_KEYBOARD_ITEM_TAB = 0x09,
    SP_INPUT_KEYBOARD_ITEM_CLEAR = 0x0C,
    SP_INPUT_KEYBOARD_ITEM_ENTER = 0x0D, // VK_RETURN
    SP_INPUT_KEYBOARD_ITEM_SHIFT = 0x10,
    SP_INPUT_KEYBOARD_ITEM_CONTROL = 0x11,
    SP_INPUT_KEYBOARD_ITEM_MENU = 0x12,
    SP_INPUT_KEYBOARD_ITEM_PAUSE = 0x13,
    SP_INPUT_KEYBOARD_ITEM_CAPSLOCK = 0x14, // VK_CAPITAL
    SP_INPUT_KEYBOARD_ITEM_KANA = 0x15,
    SP_INPUT_KEYBOARD_ITEM_JUNJA = 0x17,
    SP_INPUT_KEYBOARD_ITEM_FINAL = 0x18,
    SP_INPUT_KEYBOARD_ITEM_HANJA = 0x19,
    SP_INPUT_KEYBOARD_ITEM_KANJI = 0x19,
    SP_INPUT_KEYBOARD_ITEM_ESCAPE = 0x1B,
    SP_INPUT_KEYBOARD_ITEM_CONVERT = 0x1C, // IME
    SP_INPUT_KEYBOARD_ITEM_NONCONVERT = 0x1D, // IME
    SP_INPUT_KEYBOARD_ITEM_ACCEPT = 0x1E, // IME
    SP_INPUT_KEYBOARD_ITEM_MODECHANGE = 0x1F, // IME
    SP_INPUT_KEYBOARD_ITEM_SPACE = 0x20,
    SP_INPUT_KEYBOARD_ITEM_PAGEUP = 0x21, // VK_PRIOR
    SP_INPUT_KEYBOARD_ITEM_PAGEDOWN = 0x22, // VK_NEXT
    SP_INPUT_KEYBOARD_ITEM_END = 0x23,
    SP_INPUT_KEYBOARD_ITEM_HOME = 0x24,
    SP_INPUT_KEYBOARD_ITEM_LEFT = 0x25,
    SP_INPUT_KEYBOARD_ITEM_UP = 0x26,
    SP_INPUT_KEYBOARD_ITEM_RIGHT = 0x27,
    SP_INPUT_KEYBOARD_ITEM_DOWN = 0x28,
    SP_INPUT_KEYBOARD_ITEM_SELECT = 0x29,
    SP_INPUT_KEYBOARD_ITEM_PRINT = 0x2A,
    SP_INPUT_KEYBOARD_ITEM_EXECUTE = 0x2B,
    SP_INPUT_KEYBOARD_ITEM_PRINTSCREEN = 0x2C, // VK_SNAPSHOT, SysRq
    SP_INPUT_KEYBOARD_ITEM_INSERT = 0x2D,
    SP_INPUT_KEYBOARD_ITEM_DELETE = 0x2E,
    SP_INPUT_KEYBOARD_ITEM_HELP = 0x2F,
    SP_INPUT_KEYBOARD_ITEM_0 = 0x30,
    SP_INPUT_KEYBOARD_ITEM_1 = 0x31,
    SP_INPUT_KEYBOARD_ITEM_2 = 0x32,
    SP_INPUT_KEYBOARD_ITEM_3 = 0x33,
    SP_INPUT_KEYBOARD_ITEM_4 = 0x34,
    SP_INPUT_KEYBOARD_ITEM_5 = 0x35,
    SP_INPUT_KEYBOARD_ITEM_6 = 0x36,
    SP_INPUT_KEYBOARD_ITEM_7 = 0x37,
    SP_INPUT_KEYBOARD_ITEM_8 = 0x38,
    SP_INPUT_KEYBOARD_ITEM_9 = 0x39,
    SP_INPUT_KEYBOARD_ITEM_A = 0x41,
    SP_INPUT_KEYBOARD_ITEM_B = 0x42,
    SP_INPUT_KEYBOARD_ITEM_C = 0x43,
    SP_INPUT_KEYBOARD_ITEM_D = 0x44,
    SP_INPUT_KEYBOARD_ITEM_E = 0x45,
    SP_INPUT_KEYBOARD_ITEM_F = 0x46,
    SP_INPUT_KEYBOARD_ITEM_G = 0x47,
    SP_INPUT_KEYBOARD_ITEM_H = 0x48,
    SP_INPUT_KEYBOARD_ITEM_I = 0x49,
    SP_INPUT_KEYBOARD_ITEM_J = 0x4a,
    SP_INPUT_KEYBOARD_ITEM_K = 0x4b,
    SP_INPUT_KEYBOARD_ITEM_L = 0x4c,
    SP_INPUT_KEYBOARD_ITEM_M = 0x4d,
    SP_INPUT_KEYBOARD_ITEM_N = 0x4e,
    SP_INPUT_KEYBOARD_ITEM_O = 0x4f,
    SP_INPUT_KEYBOARD_ITEM_P = 0x50,
    SP_INPUT_KEYBOARD_ITEM_Q = 0x51,
    SP_INPUT_KEYBOARD_ITEM_R = 0x52,
    SP_INPUT_KEYBOARD_ITEM_S = 0x53,
    SP_INPUT_KEYBOARD_ITEM_T = 0x54,
    SP_INPUT_KEYBOARD_ITEM_U = 0x55,
    SP_INPUT_KEYBOARD_ITEM_V = 0x56,
    SP_INPUT_KEYBOARD_ITEM_W = 0x57,
    SP_INPUT_KEYBOARD_ITEM_X = 0x58,
    SP_INPUT_KEYBOARD_ITEM_Y = 0x59,
    SP_INPUT_KEYBOARD_ITEM_Z = 0x5a,
    SP_INPUT_KEYBOARD_ITEM_LWIN = 0x5B,
    SP_INPUT_KEYBOARD_ITEM_RWIN = 0x5C,
    SP_INPUT_KEYBOARD_ITEM_APPS = 0x5D,
    SP_INPUT_KEYBOARD_ITEM_SLEEP = 0x5F,
    SP_INPUT_KEYBOARD_ITEM_NUMPAD0 = 0x60,
    SP_INPUT_KEYBOARD_ITEM_NUMPAD1 = 0x61,
    SP_INPUT_KEYBOARD_ITEM_NUMPAD2 = 0x62,
    SP_INPUT_KEYBOARD_ITEM_NUMPAD3 = 0x63,
    SP_INPUT_KEYBOARD_ITEM_NUMPAD4 = 0x64,
    SP_INPUT_KEYBOARD_ITEM_NUMPAD5 = 0x65,
    SP_INPUT_KEYBOARD_ITEM_NUMPAD6 = 0x66,
    SP_INPUT_KEYBOARD_ITEM_NUMPAD7 = 0x67,
    SP_INPUT_KEYBOARD_ITEM_NUMPAD8 = 0x68,
    SP_INPUT_KEYBOARD_ITEM_NUMPAD9 = 0x69,
    SP_INPUT_KEYBOARD_ITEM_NUMPADASTERISK = 0x6A, // VK_MULTIPLY
    SP_INPUT_KEYBOARD_ITEM_NUMPADPLUS = 0x6B, // VK_ADD
    SP_INPUT_KEYBOARD_ITEM_NUMPADENTER = 0x6C, // VK_SEPARATOR
    SP_INPUT_KEYBOARD_ITEM_NUMPADMINUS = 0x6D, // VK_SUBTRACT
    SP_INPUT_KEYBOARD_ITEM_NUMPADDOT = 0x6E, // VK_DECIMAL
    SP_INPUT_KEYBOARD_ITEM_NUMPADSLASH = 0x6F, // VK_DIVIDE
    SP_INPUT_KEYBOARD_ITEM_F1 = 0x70,
    SP_INPUT_KEYBOARD_ITEM_F2 = 0x71,
    SP_INPUT_KEYBOARD_ITEM_F3 = 0x72,
    SP_INPUT_KEYBOARD_ITEM_F4 = 0x73,
    SP_INPUT_KEYBOARD_ITEM_F5 = 0x74,
    SP_INPUT_KEYBOARD_ITEM_F6 = 0x75,
    SP_INPUT_KEYBOARD_ITEM_F7 = 0x76,
    SP_INPUT_KEYBOARD_ITEM_F8 = 0x77,
    SP_INPUT_KEYBOARD_ITEM_F9 = 0x78,
    SP_INPUT_KEYBOARD_ITEM_F10 = 0x79,
    SP_INPUT_KEYBOARD_ITEM_F11 = 0x7A,
    SP_INPUT_KEYBOARD_ITEM_F12 = 0x7B,
    SP_INPUT_KEYBOARD_ITEM_F13 = 0x7C,
    SP_INPUT_KEYBOARD_ITEM_F14 = 0x7D,
    SP_INPUT_KEYBOARD_ITEM_F15 = 0x7E,
    SP_INPUT_KEYBOARD_ITEM_F16 = 0x7F,
    SP_INPUT_KEYBOARD_ITEM_F17 = 0x80,
    SP_INPUT_KEYBOARD_ITEM_F18 = 0x81,
    SP_INPUT_KEYBOARD_ITEM_F19 = 0x82,
    SP_INPUT_KEYBOARD_ITEM_F20 = 0x83,
    SP_INPUT_KEYBOARD_ITEM_F21 = 0x84,
    SP_INPUT_KEYBOARD_ITEM_F22 = 0x85,
    SP_INPUT_KEYBOARD_ITEM_F23 = 0x86,
    SP_INPUT_KEYBOARD_ITEM_F24 = 0x87,
    SP_INPUT_KEYBOARD_ITEM_NAVIGATION_VIEW = 0x88,
    SP_INPUT_KEYBOARD_ITEM_NAVIGATION_MENU = 0x89,
    SP_INPUT_KEYBOARD_ITEM_NAVIGATION_UP = 0x8A,
    SP_INPUT_KEYBOARD_ITEM_NAVIGATION_DOWN = 0x8B,
    SP_INPUT_KEYBOARD_ITEM_NAVIGATION_LEFT = 0x8C,
    SP_INPUT_KEYBOARD_ITEM_NAVIGATION_RIGHT = 0x8D,
    SP_INPUT_KEYBOARD_ITEM_NAVIGATION_ACCEPT = 0x8E,
    SP_INPUT_KEYBOARD_ITEM_NAVIGATION_CANCEL = 0x8F,
    SP_INPUT_KEYBOARD_ITEM_NUMLOCK = 0x90,
    SP_INPUT_KEYBOARD_ITEM_SCROLLLOCK = 0x91, // VK_SCROLL
    SP_INPUT_KEYBOARD_ITEM_NUMPADEQUAL = 0x92, // VK_OEM_NEC_EQUAL
    SP_INPUT_KEYBOARD_ITEM_OEM_FJ_JISHO = 0x92, // 'Dictionary' key
    SP_INPUT_KEYBOARD_ITEM_OEM_FJ_MASSHOU = 0x93, // 'Unregister word' key
    SP_INPUT_KEYBOARD_ITEM_OEM_FJ_TOUROKU = 0x94, // 'Register word' key
    SP_INPUT_KEYBOARD_ITEM_OEM_FJ_LOYA = 0x95, // 'Left OYAYUBI' key
    SP_INPUT_KEYBOARD_ITEM_OEM_FJ_ROYA = 0x96, // 'Right OYAYUBI' key
    SP_INPUT_KEYBOARD_ITEM_LEFTSHIFT = 0xA0, // VK_LSHIFT
    SP_INPUT_KEYBOARD_ITEM_RIGHTSHIFT = 0xA1, // VK_RSHIFT
    SP_INPUT_KEYBOARD_ITEM_LEFTCONTROL = 0xA2, // VK_LCONTROL
    SP_INPUT_KEYBOARD_ITEM_RIGHTCONTROL = 0xA3, // VK_RCONTROL
    SP_INPUT_KEYBOARD_ITEM_LEFTALT = 0xA4, // VK_LMENU
    SP_INPUT_KEYBOARD_ITEM_RIGHTALT = 0xA5, // VK_RMENU
    SP_INPUT_KEYBOARD_ITEM_BROWSER_BACK = 0xA6,
    SP_INPUT_KEYBOARD_ITEM_BROWSER_FORWARD = 0xA7,
    SP_INPUT_KEYBOARD_ITEM_BROWSER_REFRESH = 0xA8,
    SP_INPUT_KEYBOARD_ITEM_BROWSER_STOP = 0xA9,
    SP_INPUT_KEYBOARD_ITEM_BROWSER_SEARCH = 0xAA,
    SP_INPUT_KEYBOARD_ITEM_BROWSER_FAVORITES = 0xAB,
    SP_INPUT_KEYBOARD_ITEM_BROWSER_HOME = 0xAC,
    SP_INPUT_KEYBOARD_ITEM_VOLUME_MUTE = 0xAD,
    SP_INPUT_KEYBOARD_ITEM_VOLUME_DOWN = 0xAE,
    SP_INPUT_KEYBOARD_ITEM_VOLUME_UP = 0xAF,
    SP_INPUT_KEYBOARD_ITEM_MEDIA_NEXT_TRACK = 0xB0,
    SP_INPUT_KEYBOARD_ITEM_MEDIA_PREV_TRACK = 0xB1,
    SP_INPUT_KEYBOARD_ITEM_MEDIA_STOP = 0xB2,
    SP_INPUT_KEYBOARD_ITEM_MEDIA_PLAY_PAUSE = 0xB3,
    SP_INPUT_KEYBOARD_ITEM_LAUNCH_MAIL = 0xB4,
    SP_INPUT_KEYBOARD_ITEM_LAUNCH_MEDIA_SELECT = 0xB5,
    SP_INPUT_KEYBOARD_ITEM_LAUNCH_APP1 = 0xB6,
    SP_INPUT_KEYBOARD_ITEM_LAUNCH_APP2 = 0xB7,
    SP_INPUT_KEYBOARD_ITEM_SEMICOLON = 0xBA, // VK_OEM_1
    SP_INPUT_KEYBOARD_ITEM_EQUAL = 0xBB, // VK_OEM_PLUS
    SP_INPUT_KEYBOARD_ITEM_COMMA = 0xBC, // VK_OEM_COMMA
    SP_INPUT_KEYBOARD_ITEM_MINUS = 0xBD, // VK_OEM_MINUS
    SP_INPUT_KEYBOARD_ITEM_DOT = 0xBE, // VK_OEM_PERIOD
    SP_INPUT_KEYBOARD_ITEM_SLASH = 0xBF, // VK_OEM_2
    SP_INPUT_KEYBOARD_ITEM_GRAVE = 0xC0, // VK_OEM_3
    SP_INPUT_KEYBOARD_ITEM_GAMEPAD_A = 0xC3,
    SP_INPUT_KEYBOARD_ITEM_GAMEPAD_B = 0xC4,
    SP_INPUT_KEYBOARD_ITEM_GAMEPAD_X = 0xC5,
    SP_INPUT_KEYBOARD_ITEM_GAMEPAD_Y = 0xC6,
    SP_INPUT_KEYBOARD_ITEM_GAMEPAD_RIGHT_SHOULDER = 0xC7,
    SP_INPUT_KEYBOARD_ITEM_GAMEPAD_LEFT_SHOULDER = 0xC8,
    SP_INPUT_KEYBOARD_ITEM_GAMEPAD_LEFT_TRIGGER = 0xC9,
    SP_INPUT_KEYBOARD_ITEM_GAMEPAD_RIGHT_TRIGGER = 0xCA,
    SP_INPUT_KEYBOARD_ITEM_GAMEPAD_DPAD_UP = 0xCB,
    SP_INPUT_KEYBOARD_ITEM_GAMEPAD_DPAD_DOWN = 0xCC,
    SP_INPUT_KEYBOARD_ITEM_GAMEPAD_DPAD_LEFT = 0xCD,
    SP_INPUT_KEYBOARD_ITEM_GAMEPAD_DPAD_RIGHT = 0xCE,
    SP_INPUT_KEYBOARD_ITEM_GAMEPAD_MENU = 0xCF,
    SP_INPUT_KEYBOARD_ITEM_GAMEPAD_VIEW = 0xD0,
    SP_INPUT_KEYBOARD_ITEM_GAMEPAD_LEFT_THUMBSTICK_BUTTON = 0xD1,
    SP_INPUT_KEYBOARD_ITEM_GAMEPAD_RIGHT_THUMBSTICK_BUTTON = 0xD2,
    SP_INPUT_KEYBOARD_ITEM_GAMEPAD_LEFT_THUMBSTICK_UP = 0xD3,
    SP_INPUT_KEYBOARD_ITEM_GAMEPAD_LEFT_THUMBSTICK_DOWN = 0xD4,
    SP_INPUT_KEYBOARD_ITEM_GAMEPAD_LEFT_THUMBSTICK_RIGHT = 0xD5,
    SP_INPUT_KEYBOARD_ITEM_GAMEPAD_LEFT_THUMBSTICK_LEFT = 0xD6,
    SP_INPUT_KEYBOARD_ITEM_GAMEPAD_RIGHT_THUMBSTICK_UP = 0xD7,
    SP_INPUT_KEYBOARD_ITEM_GAMEPAD_RIGHT_THUMBSTICK_DOWN = 0xD8,
    SP_INPUT_KEYBOARD_ITEM_GAMEPAD_RIGHT_THUMBSTICK_RIGHT = 0xD9,
    SP_INPUT_KEYBOARD_ITEM_GAMEPAD_RIGHT_THUMBSTICK_LEFT = 0xDA,
    SP_INPUT_KEYBOARD_ITEM_LEFTBRACE = 0xDB, //  VK_OEM_4
    SP_INPUT_KEYBOARD_ITEM_BACKSLASH = 0xDC, // VK_OEM_5
    SP_INPUT_KEYBOARD_ITEM_RIGHTBRACE = 0xDD, //  VK_OEM_6
    SP_INPUT_KEYBOARD_ITEM_APOSTROPHE = 0xDE, //  VK_OEM_7
    SP_INPUT_KEYBOARD_ITEM_OEM_8 = 0xDF,
    SP_INPUT_KEYBOARD_ITEM_OEM_AX = 0xE1, //  'AX' key on Japanese AX kbd
    SP_INPUT_KEYBOARD_ITEM_OEM_102 = 0xE2, //  "<>" or "\|" on RT 102-key kbd.
    SP_INPUT_KEYBOARD_ITEM_ICO_HELP = 0xE3, //  Help key on ICO
    SP_INPUT_KEYBOARD_ITEM_ICO_00 = 0xE4, //  00 key on ICO
    SP_INPUT_KEYBOARD_ITEM_PROCESSKEY = 0xE5,
    SP_INPUT_KEYBOARD_ITEM_ICO_CLEAR = 0xE6,
    SP_INPUT_KEYBOARD_ITEM_PACKET = 0xE7,
    SP_INPUT_KEYBOARD_ITEM_OEM_RESET = 0xE9,
    SP_INPUT_KEYBOARD_ITEM_OEM_JUMP = 0xEA,
    SP_INPUT_KEYBOARD_ITEM_OEM_PA1 = 0xEB,
    SP_INPUT_KEYBOARD_ITEM_OEM_PA2 = 0xEC,
    SP_INPUT_KEYBOARD_ITEM_OEM_PA3 = 0xED,
    SP_INPUT_KEYBOARD_ITEM_OEM_WSCTRL = 0xEE,
    SP_INPUT_KEYBOARD_ITEM_OEM_CUSEL = 0xEF,
    SP_INPUT_KEYBOARD_ITEM_OEM_ATTN = 0xF0,
    SP_INPUT_KEYBOARD_ITEM_OEM_FINISH = 0xF1,
    SP_INPUT_KEYBOARD_ITEM_COPY = 0xF2,
    SP_INPUT_KEYBOARD_ITEM_OEM_AUTO = 0xF3,
    SP_INPUT_KEYBOARD_ITEM_OEM_ENLW = 0xF4,
    SP_INPUT_KEYBOARD_ITEM_OEM_BACKTAB = 0xF5,
    SP_INPUT_KEYBOARD_ITEM_ATTN = 0xF6,
    SP_INPUT_KEYBOARD_ITEM_CRSEL = 0xF7,
    SP_INPUT_KEYBOARD_ITEM_EXSEL = 0xF8,
    SP_INPUT_KEYBOARD_ITEM_EREOF = 0xF9,
    SP_INPUT_KEYBOARD_ITEM_PLAY = 0xFA,
    SP_INPUT_KEYBOARD_ITEM_ZOOM = 0xFB,
    SP_INPUT_KEYBOARD_ITEM_NONAME = 0xFC,
    SP_INPUT_KEYBOARD_ITEM_PA1 = 0xFD,
    SP_INPUT_KEYBOARD_ITEM_OEM_CLEAR = 0xFE,

    // Keys not available as Windows virtual keys.
    //
    // I've tried as best I can to match up the key codes from the OS X HID input system with
    // Windows Virtual Key Codes, but I might have missed something. I.e., some of the keys listed
    // below might be duplicates of the Windows keys above. We should test with a bunch of keyboards
    // on OS X and Windows and see that we have the best key matchup possible.

    SP_INPUT_KEYBOARD_ITEM_HASHTILDE,
    SP_INPUT_KEYBOARD_ITEM_102ND,
    SP_INPUT_KEYBOARD_ITEM_COMPOSE,
    SP_INPUT_KEYBOARD_ITEM_POWER,

    SP_INPUT_KEYBOARD_ITEM_OPEN,
    SP_INPUT_KEYBOARD_ITEM_PROPS,
    SP_INPUT_KEYBOARD_ITEM_FRONT,
    SP_INPUT_KEYBOARD_ITEM_STOP,
    SP_INPUT_KEYBOARD_ITEM_AGAIN,
    SP_INPUT_KEYBOARD_ITEM_UNDO,
    SP_INPUT_KEYBOARD_ITEM_CUT,
    SP_INPUT_KEYBOARD_ITEM_PASTE,
    SP_INPUT_KEYBOARD_ITEM_FIND,

    SP_INPUT_KEYBOARD_ITEM_NUMPADCOMMA,

    SP_INPUT_KEYBOARD_ITEM_RO, // Keyboard International1
    SP_INPUT_KEYBOARD_ITEM_KATAKANAHIRAGANA, // Keyboard International2
    SP_INPUT_KEYBOARD_ITEM_YEN, // Keyboard International3
    SP_INPUT_KEYBOARD_ITEM_HENKAN, // Keyboard International4
    SP_INPUT_KEYBOARD_ITEM_MUHENKAN, // Keyboard International5
    SP_INPUT_KEYBOARD_ITEM_NUMPADJPCOMMA, // Keyboard International6
    SP_INPUT_KEYBOARD_ITEM_INTERNATIONAL_7,
    SP_INPUT_KEYBOARD_ITEM_INTERNATIONAL_8,
    SP_INPUT_KEYBOARD_ITEM_INTERNATIONAL_9,

    SP_INPUT_KEYBOARD_ITEM_HANGEUL, // Keyboard LANG1
    SP_INPUT_KEYBOARD_ITEM_KATAKANA, // Keyboard LANG3
    SP_INPUT_KEYBOARD_ITEM_HIRAGANA, // Keyboard LANG4
    SP_INPUT_KEYBOARD_ITEM_ZENKAKUHANKAKU, // Keyboard LANG5
    SP_INPUT_KEYBOARD_ITEM_LANG_6,
    SP_INPUT_KEYBOARD_ITEM_LANG_7,
    SP_INPUT_KEYBOARD_ITEM_LANG_8,
    SP_INPUT_KEYBOARD_ITEM_LANG_9,

    SP_INPUT_KEYBOARD_ITEM_NUMPADLEFTPAREN,
    SP_INPUT_KEYBOARD_ITEM_NUMPADRIGHTPAREN,

    SP_INPUT_KEYBOARD_ITEM_LEFTMETA,
    SP_INPUT_KEYBOARD_ITEM_RIGHTMETA,

    SP_INPUT_KEYBOARD_ITEM_MEDIA_EJECT,
    SP_INPUT_KEYBOARD_ITEM_MEDIA_VOLUME_UP,
    SP_INPUT_KEYBOARD_ITEM_MEDIA_VOLUME_DOWN,
    SP_INPUT_KEYBOARD_ITEM_MEDIA_MUTE,

    SP_INPUT_KEYBOARD_ITEM_BROWSER_WWW,
    SP_INPUT_KEYBOARD_ITEM_BROWSER_SCROLLUP,
    SP_INPUT_KEYBOARD_ITEM_BROWSER_SCROLLDOWN,
    SP_INPUT_KEYBOARD_ITEM_BROWSER_EDIT,
    SP_INPUT_KEYBOARD_ITEM_BROWSER_SLEEP,
    SP_INPUT_KEYBOARD_ITEM_BROWSER_COFFEE,
    SP_INPUT_KEYBOARD_ITEM_BROWSER_CALC,

    SP_INPUT_KEYBOARD_ITEM_COUNT,
};

enum INPUT_KEY_STATE_FLAGS
{
    INPUT_KEY_STATE_FLAG_KEY_NONE = 0x00,
    INPUT_KEY_STATE_FLAG_KEY_IS_DOWN = 0x01,
    INPUT_KEY_STATE_FLAG_KEY_WAS_DOWN = 0x80,
    INPUT_KEY_STATE_FLAG_KEY_FIRST_DOWN = 0x10
};

#define MAX_SCHEME_ID 32
#define MAX_INPUT_CONTROL_ID 16
#define MAX_INPUT_SCHEME_CONTROLS 32

typedef struct input_control_t
{
	char id[MAX_INPUT_CONTROL_ID];
	uint32_t vcode;
} input_control_t;

typedef struct input_scheme_t
{
	char scheme_id[MAX_SCHEME_ID];
	uint32_t num_controls;
	input_control_t inputs[MAX_INPUT_SCHEME_CONTROLS];

} input_scheme_t;

typedef struct input_central_t
{
	input_scheme_t* active_scheme;
	uint8_t controls_state[MAX_INPUT_SCHEME_CONTROLS];

} input_central_t;

static input_central_t g_input_central_o;

void input_set_active_scheme(input_scheme_t* scheme)
{
    g_input_central_o.active_scheme = scheme;
}

void input_clear_state()
{
    for (uint32_t i = 0; i < g_input_central_o.active_scheme->num_controls; ++i)
    {
        uint8_t* input_state = &g_input_central_o.controls_state[i];
        
        if (*input_state & INPUT_KEY_STATE_FLAG_KEY_WAS_DOWN)
        {
            *input_state &= ~INPUT_KEY_STATE_FLAG_KEY_WAS_DOWN;
        }
        if (*input_state & INPUT_KEY_STATE_FLAG_KEY_FIRST_DOWN)
        {
            *input_state &= ~INPUT_KEY_STATE_FLAG_KEY_FIRST_DOWN;
        }
    }
    //g_input_central_o.controls_state
}



const input_control_t* input_scheme_add_control(input_scheme_t* scheme, const char* identifier, uint32_t vcode)
{
	if (scheme->num_controls < MAX_INPUT_SCHEME_CONTROLS - 1)
	{
        input_control_t* ic = &scheme->inputs[scheme->num_controls];
        *ic = (struct input_control_t){ .vcode = vcode };
        memcpy(ic->id, identifier, sizeof(ic->id));        
		scheme->num_controls += 1;
        return ic;
	}
    return NULL;
}

bool input_is_control_down(const input_control_t* control)
{
    if (control)
    {
        const input_scheme_t* active_scheme = g_input_central_o.active_scheme;
        size_t index = (control - active_scheme->inputs);
        if (index > 0 && index < MAX_INPUT_SCHEME_CONTROLS)
        {
            return g_input_central_o.controls_state[index] & 1;
        }                
    }
    return false;
    
}

bool input_is_control_first_down(const input_control_t* control)
{
    if (control)
    {
        const input_scheme_t* active_scheme = g_input_central_o.active_scheme;
        size_t index = (control - active_scheme->inputs);
        if (index > 0 && index < MAX_INPUT_SCHEME_CONTROLS)
        {
            return g_input_central_o.controls_state[index] & 2;
        }
    }
    return false;

}

bool input_is_control_was_down(const input_control_t* control)
{
    if (control)
    {
        const input_scheme_t* active_scheme = g_input_central_o.active_scheme;
        size_t index = (control - active_scheme->inputs);
        if (index > 0 && index < MAX_INPUT_SCHEME_CONTROLS)
        {
            return g_input_central_o.controls_state[index] & 4;
        }
    }
    return false;

}

///


inline bool get_attribute_as_bool(sp_config_i* config, sp_config_item_t item, sp_strhash_t hash)
{
    sp_config_item_t attrib = config->object_get(config->inst, item, hash);    
    return attrib.type == SP_CONFIG_TYPE_TRUE;
}


void sapphire_destroy()
{
    rendering_context_destroy(g_rendering_context_o);
    
}

void sapphire_update(double curr_time, double elapsed_time)
{

}


// Render a frame
void sapphire_render(IDeviceContext* pContext)
{
    renderer_do_rendering(pContext, &g_viewer);
}


void sapphire_init(IRenderDevice* p_device, ISwapChain* p_swap_chain)
{
    scene_def_t scene_def;
    memset(&scene_def, 0, sizeof(scene_def));
    sp_allocator_i* allocator = sp_allocator_api->system_allocator;

    
    SP_INIT_TEMP_ALLOCATOR_WITH_ADAPTER(ta, a);

   
    rendering_context_create(p_device, p_swap_chain);

    g_viewer = (viewer_t){ .camera = {.near_plane = 0.1f, .far_plane = 100.f, .vertical_fov =  (SP_PI / 4.0f) } };
    viewer_t* viewer = &g_viewer;

    viewer->camera_transform = (sp_transform_t){ .position = {0, 1, -15}, .rotation = {0,0,0,1}, .scale = {1,1,1} };

    const SwapChainDesc* pSCDesc = ISwapChain_GetDesc(p_swap_chain);
    float aspect = (float)pSCDesc->Width / (float)pSCDesc->Height;
    
    // calculate view from transform
    sp_camera_api->view_from_transform(&viewer->camera.view[SP_CAMERA_TRANSFORM_DEFAULT], &viewer->camera_transform);
    // calculate projection matrix
    sp_camera_api->projection_from_fov(&viewer->camera.projection[SP_CAMERA_TRANSFORM_DEFAULT], viewer->camera.near_plane, viewer->camera.far_plane, viewer->camera.vertical_fov, aspect);

    sp_mat4x4_mul(&viewer->view_projection, &viewer->camera.view[SP_CAMERA_TRANSFORM_DEFAULT], &viewer->camera.projection[SP_CAMERA_TRANSFORM_DEFAULT]);


    sp_material_def_t* mat_defs_array = NULL;
    
    // load materials definitions from file    
    load_materials("C:/Programming/Sapphire/assets/materials.mat" , &mat_defs_array, allocator);
    // create GPU resources for materials
    material_manager_add_materials(&g_rendering_context_o->materials_manager, mat_defs_array);

    sp_array_free(mat_defs_array, allocator);

    scene_load_file("C:/Programming/Sapphire/assets/test.scene", &scene_def, allocator);
    scene_load_resources("C:/Programming/Sapphire/assets", &scene_def, p_device);
    scene_free(&scene_def, allocator);

#if 0
   
    
    
#if 1
    sapphire_mesh_gpu_load_t mesh_load_data;
    sapphire_mesh_gpu_load_t mesh_load_data2;
    const char* mesh_file = "C:/Programming/Sapphire/assets/dungeon_wall_01.model";
    const char* mesh_file2 = "C:/Programming/Sapphire/assets/barrel_crate_block.model";
    
    uint8_t* stream = NULL;
    uint64_t size;
    read_file_stream(mesh_file, ta, &stream, &size);
    read_grimrock_model_from_stream(stream, size, &mesh_load_data);

    read_file_stream(mesh_file2, ta, &stream, &size);
    read_grimrock_model_from_stream(stream, size, &mesh_load_data2);
#else
    // cube test vertices

    typedef struct Vertex
    {
        sp_vec3_t pos;
        sp_vec3_t normal;
        sp_vec2_t uv;
    } Vertex;

    Vertex CubeVerts[] =
    {
        {.pos = {-1,-1,-1}, .normal = {0, 0, -1}, .uv = {0,1}},
        {.pos = {-1,+1,-1}, .normal = {0, 0, -1},.uv = {0,0}},
        {.pos = {+1,+1,-1}, .normal = {0, 0, -1},.uv = {1,0}},
        {.pos = {+1,-1,-1}, .normal = {0, 0, -1},.uv = {1,1}},

        {.pos = {-1,-1,-1}, .normal = {0, -1, 0},.uv = {0,1}},
        {.pos = {-1,-1,+1}, .normal = {0, -1, 0},.uv = {0,0}},
        {.pos = {+1,-1,+1}, .normal = {0, -1, 0},.uv = {1,0}},
        {.pos = {+1,-1,-1}, .normal = {0, -1, 0},.uv = {1,1}},

        {.pos = {+1,-1,-1}, .normal = {1, 0, 0},.uv = {0,1} },
        {.pos = {+1,-1,+1}, .normal = {1, 0, 0},.uv = {1,1}},
        {.pos = {+1,+1,+1}, .normal = {1, 0, 0},.uv = {1,0}},
        {.pos = {+1,+1,-1}, .normal = {1, 0, 0},.uv = {0,0}},

        {.pos = {+1,+1,-1}, .normal = {0, 1, 0}, .uv = {0,1}},
        {.pos = {+1,+1,+1}, .normal = {0, 1, 0},.uv = {0,0}},
        {.pos = {-1,+1,+1}, .normal = {0, 1, 0},.uv = {1,0}},
        {.pos = {-1,+1,-1}, .normal = {0, 1, 0},.uv = {1,1}},

        {.pos = {-1,+1,-1}, .normal = {-1, 0, 0},.uv = {1,0}},
        {.pos = {-1,+1,+1}, .normal = {-1, 0, 0},.uv = {0,0}},
        {.pos = {-1,-1,+1}, .normal = {-1, 0, 0},.uv = {0,1}},
        {.pos = {-1,-1,-1}, .normal = {-1, 0, 0},.uv = {1,1}},

        {.pos = {-1,-1,+1}, .normal = {0, 0, 1},.uv = {1,1}},
        {.pos = {+1,-1,+1}, .normal = {0, 0, 1},.uv = {0,1}},
        {.pos = {+1,+1,+1}, .normal = {0, 0, 1},.uv = {0,0}},
        {.pos = {-1,+1,+1}, .normal = {0, 0, 1},.uv = {1,0}}
    };

    Uint32 Indices[] =
    {
        2,0,1,    2,3,0,
        4,6,5,    4,7,6,
        8,10,9,   8,11,10,
        12,14,13, 12,15,14,
        16,18,17, 16,19,18,
        20,21,22, 20,22,23
    };
    memset(&mesh_load_data, 0, sizeof(mesh_load_data));
    mesh_load_data.num_indices = 36;
    mesh_load_data.num_submeshes = 1;
    mesh_load_data.num_vertices = 24;
    mesh_load_data.vertex_stride = 32;
    mesh_load_data.vertices_data_size = mesh_load_data.num_vertices * mesh_load_data.vertex_stride;
    mesh_load_data.indices_data_size = sizeof(uint32_t) * mesh_load_data.num_indices;
    mesh_load_data.indices = (const uint8_t*)Indices;
    mesh_load_data.vertices[0] = (const uint8_t*)CubeVerts;
    mesh_load_data.vertex_stream_stride_size[0] = mesh_load_data.vertex_stride;
    mesh_load_data.sub_meshes[0].indices_start = 0;
    mesh_load_data.sub_meshes[0].indices_count = 36;

    sp_strhash_t mat_hash = sp_murmur_hash_string("test_mat_1");
    mesh_load_data.sub_meshes[0].material_hash = mat_hash; 

    
#endif
    sp_mesh_handle_t mesh_handle = load_mesh_to_gpu(p_device, g_rendering_context_o, &mesh_load_data);
    sp_mesh_handle_t mesh_handle2 = load_mesh_to_gpu(p_device, g_rendering_context_o, &mesh_load_data2);
    // add test instances
    sp_vec3_t axis = { 0, 1, 0 };
    sp_vec4_t q = sp_quaternion_from_rotation(axis, 0);
    sp_mat4x4_t inst_mat;
    sp_mat4x4_from_quaternion(&inst_mat, q);
    inst_mat.wz = 0.0f;
    inst_mat.wx = 0.0f;
    g_rendering_context_o->renderer.mesh_handles[0] = mesh_handle;
    g_rendering_context_o->renderer.world_matrices[0] = inst_mat;
    g_rendering_context_o->renderer.mesh_handles[1] = mesh_handle2;
    inst_mat.wz = 0.0f;
    inst_mat.wx = 2.0f;
    g_rendering_context_o->renderer.world_matrices[1] = inst_mat;
    g_rendering_context_o->renderer.num_render_objects = 2;

#endif

    SP_SHUTDOWN_TEMP_ALLOCATOR(ta);

}



void testcimgui()
{
	input_scheme_t is = {.scheme_id = "oferdklfjsd;fjs"};

    //sp_allocator_i* allocator = sp_allocator_api->system_allocator;
    //material_manager_t mm = { .material_name_lookup = {.allocator = allocator } };
    //uint64_t hash_key = sp_murmur_hash_string("xadvance");
    //sp_hash_add(&mm.material_name_lookup, hash_key, 10);
    //uint32_t val = sp_hash_get(&mm.material_name_lookup, hash_key);
    //printf("%d", val);
    
    
	ImVec2 v = { 0 };
	if (im_Button("kaki", v))
	{
		printf(is.scheme_id);
	}
}