/********** Bluetooth HID keyboard and mouse **********

Download from https://github.com/petzval/btferret
  btlib.c    Version 20 or later
  btlib.h    Version 20 or later
  keymouse.c  this code
  keymouse.txt
   
Edit keymouse.txt to set ADDRESS=
to the address of the local device
that runs this code

The Linux input devices are set by KEYBOARD_DEVICE and MOUSE_DEVICE below.
These defaults use the SteelSeries Prime keyboard and mouse.
If a USB switch disconnects them, the program retries opening them every second.

Compile
  gcc keymouse.c btlib.c -o keymouse
  
Run
  sudo ./keymouse

Connect from phone/tablet/PC to "HID" device

Keystrokes and mouse data sent to client
F10 sends "Hello" plus Enter
The mouse moves slightly right, left, up and down every 20 seconds

This code sets an unchanging random address.
If connection is unreliable try changing the address.

See HID Devices section in documentation for 
more infomation.

*********************************/    

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/input.h>
#include <string.h>
#include "btlib.h"  

int lecallback(int clientnode,int op,int cticn);
int send_key(int key);
int send_mouse(char x,char y,char but,char wheel);

#define KEYBOARD_DEVICE "/dev/input/by-id/usb-SteelSeries_SteelSeries_Apex_3-event-kbd"
#define MOUSE_DEVICE "/dev/input/by-id/usb-SteelSeries_SteelSeries_Prime-if01-event-mouse"
#define INPUT_RETRY_TICKS 50
#define MOUSE_JIGGLE_INTERVAL_TICKS 1000

unsigned char reportmap[101] = {0x05,0x01,0x09,0x06,0xA1,0x01,0x85,0x01,0x05,0x07,0x19,0xE0,0x29,0xE7,0x15,0x00,
                               0x25,0x01,0x75,0x01,0x95,0x08,0x81,0x02,0x95,0x01,0x75,0x08,0x81,0x01,0x95,0x06,
                               0x75,0x08,0x15,0x00,0x25,0x65,0x05,0x07,0x19,0x00,0x29,0x65,0x81,0x00,0xC0,
                               0x05,0x01,0x09,0x02,0xA1,0x01,0x85,0x02,0x09,0x01,0xA1,0x00,0x05,0x09,0x19,0x01,
                               0x29,0x05,0x15,0x00,0x25,0x01,0x95,0x05,0x75,0x01,0x81,0x02,0x95,0x01,0x75,0x03,
                               0x81,0x01,0x05,0x01,0x09,0x30,0x09,0x31,0x09,0x38,0x15,0x81,0x25,0x7F,0x75,0x08,
                               0x95,0x03,0x81,0x06,0xC0,0xC0};

unsigned char report[8] = {0,0,0,0,0,0,0,0};
unsigned char *name = "HID"; 
unsigned char appear[2] = {0xC1,0x03};  // 03C1 = keyboard icon appears on connecting device 
unsigned char pnpinfo[7] = {0x02,0x6B,0x1D,0x46,0x02,0x37,0x05};
unsigned char protocolmode[1] = {0x01};
unsigned char hidinfo[4] = {0x01,0x11,0x00,0x02};
int keyboard_fd = -1,mouse_fd = -1;
unsigned char keyboard_modifiers = 0;
unsigned char keyboard_keys[6] = {0,0,0,0,0,0};
unsigned char mouse_buttons = 0;
int keyboard_retry_ticks = 0,mouse_retry_ticks = 0;
int mouse_jiggle_ticks = 0,mouse_jiggle_step = 0;

int linux_key_to_hid(int key);
int send_keyboard_report(void);
void read_keyboard(void);
void read_mouse(void);
void jiggle_mouse(void);
void close_input_devices(void);
void open_input_devices(int log_failures);
void retry_input_devices(void);

int main()
  {
  unsigned char uuid[2],randadd[6];
   
  if(init_blue("keymouse.txt") == 0)
    return(0);
    
  if(localnode() != 1)
    {
    printf("ERROR - Edit keymouse.txt to set ADDRESS = %s\n",device_address(localnode()));
    return(0);
    }

  // Write data to local characteristics
  uuid[0] = 0x2A;
  uuid[1] = 0x00;
  write_ctic(localnode(),find_ctic_index(localnode(),UUID_2,uuid),name,3); 

  uuid[0] = 0x2A;
  uuid[1] = 0x01;
  write_ctic(localnode(),find_ctic_index(localnode(),UUID_2,uuid),appear,0);

  uuid[0] = 0x2A;
  uuid[1] = 0x4E;
  write_ctic(localnode(),find_ctic_index(localnode(),UUID_2,uuid),protocolmode,0);

  uuid[0] = 0x2A;
  uuid[1] = 0x4A;
  write_ctic(localnode(),find_ctic_index(localnode(),UUID_2,uuid),hidinfo,0);

  uuid[0] = 0x2A;
  uuid[1] = 0x4B;
  write_ctic(localnode(),find_ctic_index(localnode(),UUID_2,uuid),reportmap,0);

  uuid[0] = 0x2A;
  uuid[1] = 0x4D;
  write_ctic(localnode(),find_ctic_index(localnode(),UUID_2,uuid),report,0);

  uuid[0] = 0x2A;
  uuid[1] = 0x50;
  write_ctic(localnode(),find_ctic_index(localnode(),UUID_2,uuid),pnpinfo,0);
                            
  // Set unchanging random address by hard-coding a fixed value.
  // Choose the following 6 numbers
  randadd[0] = 0xD3;  // 2 hi bits must be 1
  randadd[1] = 0x56;
  randadd[2] = 0xD6;
  randadd[3] = 0x74;
  randadd[4] = 0x33;
  randadd[5] = 0x06;
  set_le_random_address(randadd);
     
  keys_to_callback(KEY_ON,0);  // enable LE_KEYPRESS calls in lecallback
                               // 0 = GB keyboard  
  set_le_wait(20000);  // Allow 20 seconds for connection to complete
                                         
  le_pair(localnode(),JUST_WORKS,0);  // Easiest option, but if client requires
                                      // passkey security - remove this command  
  set_flags(FAST_TIMER,FLAG_ON);
  le_server(lecallback,20);  // 20ms FAST_TIMER

  close_input_devices();
  
  close_all();
  return(1);
  }


int lecallback(int clientnode,int op,int cticn)
  {
  if(op == LE_CONNECT)
    {
    close_input_devices();
    open_input_devices(1);
    if(keyboard_fd >= 0 && mouse_fd >= 0)
      printf("Connected OK. Keyboard and mouse sent to client. ESC stops server\n");
    else if(keyboard_fd >= 0)
      printf("Connected OK. Keyboard sent to client; mouse unavailable. ESC stops server\n");
    else if(mouse_fd >= 0)
      printf("Connected OK. Mouse sent to client; keyboard unavailable. ESC stops server\n");
    else
      printf("Connected OK. Keyboard and mouse unavailable. ESC stops server\n");
    }
  if(op == LE_TIMER)
    {
    retry_input_devices();
    read_keyboard();
    read_mouse();
    jiggle_mouse();
    }
  if(op == LE_DISCONNECT)
    {
    close_input_devices();
    return(SERVER_EXIT);
    }
  return(SERVER_CONTINUE);
  }

void close_input_devices(void)
  {
  if(keyboard_fd >= 0)
    close(keyboard_fd);
  if(mouse_fd >= 0)
    close(mouse_fd);
  keyboard_fd = -1;
  mouse_fd = -1;
  keyboard_retry_ticks = 0;
  mouse_retry_ticks = 0;
  keyboard_modifiers = 0;
  memset(keyboard_keys,0,sizeof(keyboard_keys));
  mouse_buttons = 0;
  mouse_jiggle_ticks = 0;
  mouse_jiggle_step = 0;
  }

void open_input_devices(int log_failures)
  {
  if(keyboard_fd < 0)
    {
    keyboard_fd = open(KEYBOARD_DEVICE,O_RDONLY | O_NONBLOCK);
    if(keyboard_fd >= 0)
      printf("Opened keyboard input %s\n",KEYBOARD_DEVICE);
    else if(log_failures)
      printf("Failed to open %s: %s (will retry)\n",KEYBOARD_DEVICE,strerror(errno));
    }
  if(mouse_fd < 0)
    {
    mouse_fd = open(MOUSE_DEVICE,O_RDONLY | O_NONBLOCK);
    if(mouse_fd >= 0)
      printf("Opened mouse input %s\n",MOUSE_DEVICE);
    else if(log_failures)
      printf("Failed to open %s: %s (will retry)\n",MOUSE_DEVICE,strerror(errno));
    }
  }

void retry_input_devices(void)
  {
  if(keyboard_fd < 0 && ++keyboard_retry_ticks >= INPUT_RETRY_TICKS)
    {
    keyboard_retry_ticks = 0;
    open_input_devices(0);
    }
  if(mouse_fd < 0 && ++mouse_retry_ticks >= INPUT_RETRY_TICKS)
    {
    mouse_retry_ticks = 0;
    open_input_devices(0);
    }
  }

int linux_key_to_hid(int key)
  {
  switch(key)
    {
    case KEY_A: return(0x04);
    case KEY_B: return(0x05);
    case KEY_C: return(0x06);
    case KEY_D: return(0x07);
    case KEY_E: return(0x08);
    case KEY_F: return(0x09);
    case KEY_G: return(0x0A);
    case KEY_H: return(0x0B);
    case KEY_I: return(0x0C);
    case KEY_J: return(0x0D);
    case KEY_K: return(0x0E);
    case KEY_L: return(0x0F);
    case KEY_M: return(0x10);
    case KEY_N: return(0x11);
    case KEY_O: return(0x12);
    case KEY_P: return(0x13);
    case KEY_Q: return(0x14);
    case KEY_R: return(0x15);
    case KEY_S: return(0x16);
    case KEY_T: return(0x17);
    case KEY_U: return(0x18);
    case KEY_V: return(0x19);
    case KEY_W: return(0x1A);
    case KEY_X: return(0x1B);
    case KEY_Y: return(0x1C);
    case KEY_Z: return(0x1D);
    }
  if(key >= KEY_1 && key <= KEY_9)
    return(0x1E + key - KEY_1);
  if(key == KEY_0)
    return(0x27);
  if(key >= KEY_F1 && key <= KEY_F10)
    return(0x3A + key - KEY_F1);

  switch(key)
    {
    case KEY_F11: return(0x44);
    case KEY_F12: return(0x45);
    case KEY_ENTER: return(0x28);
    case KEY_ESC: return(0x29);
    case KEY_BACKSPACE: return(0x2A);
    case KEY_TAB: return(0x2B);
    case KEY_SPACE: return(0x2C);
    case KEY_MINUS: return(0x2D);
    case KEY_EQUAL: return(0x2E);
    case KEY_LEFTBRACE: return(0x2F);
    case KEY_RIGHTBRACE: return(0x30);
    case KEY_BACKSLASH: return(0x31);
    case KEY_102ND: return(0x32);
    case KEY_SEMICOLON: return(0x33);
    case KEY_APOSTROPHE: return(0x34);
    case KEY_GRAVE: return(0x35);
    case KEY_COMMA: return(0x36);
    case KEY_DOT: return(0x37);
    case KEY_SLASH: return(0x38);
    case KEY_CAPSLOCK: return(0x39);
    case KEY_SYSRQ: return(0x46);
    case KEY_SCROLLLOCK: return(0x47);
    case KEY_PAUSE: return(0x48);
    case KEY_INSERT: return(0x49);
    case KEY_HOME: return(0x4A);
    case KEY_PAGEUP: return(0x4B);
    case KEY_DELETE: return(0x4C);
    case KEY_END: return(0x4D);
    case KEY_PAGEDOWN: return(0x4E);
    case KEY_RIGHT: return(0x4F);
    case KEY_LEFT: return(0x50);
    case KEY_DOWN: return(0x51);
    case KEY_UP: return(0x52);
    case KEY_NUMLOCK: return(0x53);
    case KEY_KPSLASH: return(0x54);
    case KEY_KPASTERISK: return(0x55);
    case KEY_KPMINUS: return(0x56);
    case KEY_KPPLUS: return(0x57);
    case KEY_KPENTER: return(0x58);
    case KEY_KP1: return(0x59);
    case KEY_KP2: return(0x5A);
    case KEY_KP3: return(0x5B);
    case KEY_KP4: return(0x5C);
    case KEY_KP5: return(0x5D);
    case KEY_KP6: return(0x5E);
    case KEY_KP7: return(0x5F);
    case KEY_KP8: return(0x60);
    case KEY_KP9: return(0x61);
    case KEY_KP0: return(0x62);
    case KEY_KPDOT: return(0x63);
    default: return(0);
    }
  }

int send_keyboard_report(void)
  {
  int n;
  unsigned char uuid[2],buf[8];
  static int reportindex = -1;

  if(reportindex < 0)
    {
    uuid[0] = 0x2A;
    uuid[1] = 0x4D;
    reportindex = find_ctic_index(localnode(),UUID_2,uuid);
    if(reportindex < 0)
      {
      printf("Failed to find Keyboard Report characteristic\n");
      return(0);
      }
    }

  buf[0] = keyboard_modifiers;
  buf[1] = 0;
  for(n = 0 ; n < 6 ; ++n)
    buf[n + 2] = keyboard_keys[n];
  write_ctic(localnode(),reportindex,buf,0);
  return(1);
  }

void read_keyboard(void)
  {
  int nread,n,usage,slot,changed;
  unsigned char modifier;
  struct input_event event;

  if(keyboard_fd < 0)
    return;

  while(1)
    {
    nread = read(keyboard_fd,&event,sizeof(event));
    if(nread < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
      break;
    if(nread < 0 && errno == EINTR)
      continue;
    if(nread != sizeof(event))
      {
      if(nread < 0)
        printf("Read failed on %s: %s\n",KEYBOARD_DEVICE,strerror(errno));
      else
        printf("Unexpected end of input from %s\n",KEYBOARD_DEVICE);
      close(keyboard_fd);
      keyboard_fd = -1;
      keyboard_modifiers = 0;
      memset(keyboard_keys,0,sizeof(keyboard_keys));
      send_keyboard_report();
      break;
      }
    if(event.type != EV_KEY || event.value == 2)
      continue;

    changed = 0;
    modifier = 0;
    switch(event.code)
      {
      case KEY_LEFTCTRL: modifier = 0x01; break;
      case KEY_LEFTSHIFT: modifier = 0x02; break;
      case KEY_LEFTALT: modifier = 0x04; break;
      case KEY_LEFTMETA: modifier = 0x08; break;
      case KEY_RIGHTCTRL: modifier = 0x10; break;
      case KEY_RIGHTSHIFT: modifier = 0x20; break;
      case KEY_RIGHTALT: modifier = 0x40; break;
      case KEY_RIGHTMETA: modifier = 0x80; break;
      }
    if(modifier != 0)
      {
      if(event.value)
        keyboard_modifiers |= modifier;
      else
        keyboard_modifiers &= (unsigned char)~modifier;
      changed = 1;
      }
    else
      {
      usage = linux_key_to_hid(event.code);
      if(usage == 0)
        continue;
      slot = -1;
      for(n = 0 ; n < 6 ; ++n)
        {
        if(keyboard_keys[n] == usage)
          slot = n;
        }
      if(event.value && slot < 0)
        {
        for(n = 0 ; n < 6 ; ++n)
          {
          if(keyboard_keys[n] == 0)
            {
            keyboard_keys[n] = usage;
            changed = 1;
            break;
            }
          }
        }
      else if(!event.value && slot >= 0)
        {
        keyboard_keys[slot] = 0;
        changed = 1;
        }
      }
    if(changed)
      send_keyboard_report();
    }
  }

void read_mouse(void)
  {
  int nread,dx = 0,dy = 0,dwheel = 0,changed = 0;
#ifdef REL_WHEEL_HI_RES
  int hires_wheel = 0,hires_seen = 0;
  static int hires_wheel_remainder = 0;
#endif
  struct input_event event;

  if(mouse_fd < 0)
    return;

  while(1)
    {
    nread = read(mouse_fd,&event,sizeof(event));
    if(nread < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
      break;
    if(nread < 0 && errno == EINTR)
      continue;
    if(nread != sizeof(event))
      {
      if(nread < 0)
        printf("Read failed on %s: %s\n",MOUSE_DEVICE,strerror(errno));
      else
        printf("Unexpected end of input from %s\n",MOUSE_DEVICE);
      close(mouse_fd);
      mouse_fd = -1;
      mouse_buttons = 0;
      send_mouse(0,0,0,0);
      return;
      }
    if(event.type == EV_REL)
      {
      if(event.code == REL_X)
        dx += event.value;
      else if(event.code == REL_Y)
        dy += event.value;
      else if(event.code == REL_WHEEL)
        dwheel += event.value;
#ifdef REL_WHEEL_HI_RES
      else if(event.code == REL_WHEEL_HI_RES)
        {
        hires_wheel += event.value;
        hires_seen = 1;
        }
#endif
      }
    else if(event.type == EV_KEY)
      {
      if(event.code == BTN_LEFT)
        {
        if(event.value) mouse_buttons |= 1;
        else mouse_buttons &= (unsigned char)~1;
        changed = 1;
        }
      else if(event.code == BTN_RIGHT)
        {
        if(event.value) mouse_buttons |= 2;
        else mouse_buttons &= (unsigned char)~2;
        changed = 1;
        }
      else if(event.code == BTN_MIDDLE)
        {
        if(event.value) mouse_buttons |= 4;
        else mouse_buttons &= (unsigned char)~4;
        changed = 1;
        }
      else if(event.code == BTN_SIDE)
        {
        if(event.value) mouse_buttons |= 8;
        else mouse_buttons &= (unsigned char)~8;
        changed = 1;
        }
      else if(event.code == BTN_EXTRA)
        {
        if(event.value) mouse_buttons |= 16;
        else mouse_buttons &= (unsigned char)~16;
        changed = 1;
        }
      }
    }
#ifdef REL_WHEEL_HI_RES
  if(hires_seen)
    {
    hires_wheel += hires_wheel_remainder;
    dwheel = hires_wheel / 120;
    hires_wheel_remainder = hires_wheel % 120;
    }
#endif
  if(dx != 0 || dy != 0 || dwheel != 0 || changed)
    {
    if(dx > 127) dx = 127;
    else if(dx < -127) dx = -127;
    if(dy > 127) dy = 127;
    else if(dy < -127) dy = -127;
    if(dwheel > 127) dwheel = 127;
    else if(dwheel < -127) dwheel = -127;
    send_mouse((char)dx,(char)dy,mouse_buttons,(char)dwheel);
    }
  }

void jiggle_mouse(void)
  {
  if(mouse_jiggle_step == 0)
    {
    if(++mouse_jiggle_ticks < MOUSE_JIGGLE_INTERVAL_TICKS)
      return;
    mouse_jiggle_ticks = 0;
    mouse_jiggle_step = 1;
    }

  switch(mouse_jiggle_step)
    {
    case 1: send_mouse(1,0,(char)mouse_buttons,0); break;
    case 2: send_mouse(-1,0,(char)mouse_buttons,0); break;
    case 3: send_mouse(0,-1,(char)mouse_buttons,0); break;
    case 4: send_mouse(0,1,(char)mouse_buttons,0); break;
    }
  if(++mouse_jiggle_step > 4)
    mouse_jiggle_step = 0;
  }

/*********** SEND KEY *****************

send_key(key code);

key code = ASCII code of character (e.g a=97) OR one of the
           following btferret custom codes:

1 = Pause     8 = Backspace  17 = F4     24 = F11
2 = Insert    9 = Tab        18 = F5     25 = F12
3 = Del      10 = Enter      19 = F6     27 = Esc
4 = Home     11 = Pound (^3) 20 = F7     28 = Right arrow
5 = End      14 = F1         21 = F8     29 = Left arrow
6 = PgUp     15 = F2         22 = F9     30 = Down arrow
7 = PgDn     16 = F3         23 = F10    31 = Up arrow 

ASCII codes                    'a' = 97               (valid range 32-126)
CTRL add 128 (0x80)         CTRL a = 'a' + 128 = 225  (valid range 225-255)
Left ALT add 256 (0x100)     ALT a = 'a' + 256 = 353  (valid range 257-382)
Right ALT add 384 (0x180)  AltGr a = 'a' + 384 = 481  (valid range 481-516)

SHIFT F1-F8 codes SHIFT F1 = 471  (valid range 471-478) 

Note CTRL i = same as Tab  CTRL m = same as Enter  
Some ALT keys generate ASCII codes

To send k: send_key('k')  
To send F1: send_key(14)
To send CTRL b:  send_key(226) same as send_key('b' | 0x80)
To send AltGr t: send_key(500) same as send_key('t' | 0x180)

These key codes are also listed in the
keys_to_callback() section in documentation

Modifier bits, hex values:
01=Left CTL  02=Left Shift  04=Left Alt  08=Left GUI
10=Right CTL 20=Right Shift 40=Right Alt 80=Right GUI
 
**************************************/

int send_key(int key)
  {
  int n,hidcode;
  unsigned char buf[8];
  static int reportindex = -1;

  // convert btferret code (key) to HID code  
  hidcode = hid_key_code(key);
  if(hidcode == 0)
    return(0);

  if(reportindex < 0)
    {  // look up Report1 index
    buf[0] = 0x2A;
    buf[1] = 0x4D;
    reportindex = find_ctic_index(localnode(),UUID_2,buf);
    if(reportindex < 0)
      {
      printf("Failed to find Report characteristic\n");
      return(0);
      }
    }     
 
  for(n = 0 ; n < 8 ; ++n)
    buf[n] = 0;
        
  // send key press to Report1
  buf[0] = (hidcode >> 8) & 0xFF;  // modifier
  buf[2] = hidcode & 0xFF;         // key code
  write_ctic(localnode(),reportindex,buf,0);
  // send no key pressed - all zero
  buf[0] = 0;
  buf[2] = 0;
  write_ctic(localnode(),reportindex,buf,0); 
  return(1);
  }

/*********** SEND MOUSE *********
x,y = mouse movement -127 to 127
wheel = vertical scrolling -127 to 127

but  1 = Left button click
     2 = Right button click
     4 = Middle button click
     8 = Side button click
    16 = Extra button click
  
  For a single click these button presses are followed
  by a buf[0]=0 send which indicates button up.      
********************************/

int send_mouse(char x,char y,char but,char wheel)
  {
  unsigned char buf[4];
  static int reportindex = -1;
  
  if(reportindex < 0)
    {  // look up Report 1 index
    buf[0] = 0x2A;
    buf[1] = 0x4D;
    reportindex = find_ctic_index(localnode(),UUID_2,buf);
    if(reportindex < 0)
      {
      printf("Failed to find Report characteristic\n");
      return(0);
      }
    ++reportindex;  // Mouse is Report 2
    }     
 
  buf[0] = but;
  buf[1] = x;
  buf[2] = y;
  buf[3] = wheel;
        
  // send to Report2
  write_ctic(localnode(),reportindex,buf,0);

  return(1);
  }
