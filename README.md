# Improved srcds v34
Improved Counter-Strike Source v34 Dedicated Server also known as srcds fix in CSS v34 community.
## Description
This is the open source server part of the game Counter-Strike: Source v34 based on the Source SDK Base 2006 and Source Engine 2007 with changes for the latest build of the game as well as some improvements.
## Improvements
* Physics fixes for 100 tickrate
* Fixed some console commands leading to the crash of the server
* Added new console commands that extend gameplay and fix bugs
* Optimization using SSE
## ConVars
* sv_maxusrcmdprocessticks <16> - Maximum number of client-issued usrcmd ticks that can be replayed in packet loss conditions, 0 to allow no restrictions
* sv_maxusrcmdprocessticks_warning <-1> - Print a warning when user commands get dropped due to insufficient usrcmd ticks allocated, number of seconds to throttle, negative disabled
* sv_maxusrcmdprocessticks_holdaim <1> - Hold client aim for multiple server sim ticks when client-issued usrcmd contains multiple actions (0: off; 1: hold this server tick; 2+: hold multiple ticks)
* sv_player_usercommand_timeout <3> - After this many seconds without a usercommand from a player, the server will RunNullCommand as if client sends an empty command
* sv_usercmd_custom_random_seed <1> - When enabled server will populate an additional random seed independent of the client
* sv_max_usercmd_move_magnitude <1000> - Maximum move magnitude that can be requested by client
* cs_enable_player_physics_box <1> - Allow to create physical objects for players
* mp_ignore_round_win_conditions <0> - Ignore conditions which would end the current round
* mp_round_restart_delay <5.0> - Number of seconds to delay before restarting a round after a win
* sv_enablebunnyhopping <0> - Allow player speed to exceed maximum running speed
## Building
To compile, you need to use <b>GCC 3.4.6</b> or lower. First, go to the <b>linux_sdk</b> directory and open the <b>Makefile</b>, then edit it for your system. Type <b>make</b> to build. 
After compilation, the binary file will be located in the working directory.
