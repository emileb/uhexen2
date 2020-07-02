LOCAL_PATH:= $(call my-dir)/../engine/

include $(CLEAR_VARS)

LOCAL_MODULE := uhexen2

LOCAL_CFLAGS := -DUHEXEN2 -DENGINE_NAME=\"Hexen2\" -DGLQUAKE -DGL_DLSYM -DSDLQUAKE -DUSE_CODEC_MP3 -DUSE_CODEC_TIMIDITY -DTIMIDITY_STATIC=1 -DUSE_CODEC_VORBIS -DNO_ALSA_AUDIO -DNO_OSS_AUDIO
LOCAL_CPPFLAGS :=

LOCAL_C_INCLUDES :=     $(SDL_INCLUDE_PATHS)  \
                        $(TOP_DIR) \
                        $(TOP_DIR)/MobileTouchControls \
                        $(TOP_DIR)/MobileTouchControls/libpng \
                        $(TOP_DIR)/AudioLibs_OpenTouch/liboggvorbis/include \
                        $(TOP_DIR)/AudioLibs_OpenTouch/ \
                        $(TOP_DIR)/AudioLibs_OpenTouch/libmad \
                        $(TOP_DIR)/Clibs_OpenTouch \
                        $(TOP_DIR)/Clibs_OpenTouch/jpeg8d \
                        $(TOP_DIR)/Clibs_OpenTouch/freetype2-android/include \
                        $(TOP_DIR)/Clibs_OpenTouch/quake \
                        $(TOP_DIR)/gl4es/include \
                        $(LOCAL_PATH)/hexen2 \
                        $(LOCAL_PATH)/h2shared \
                        $(LOCAL_PATH)/../common \
                        $(LOCAL_PATH)/../libs/timidity \


LOCAL_SRC_FILES :=	../mobile/game_interface.c \
                    ../../../Clibs_OpenTouch/quake/android_jni.cpp \
                    ../../../Clibs_OpenTouch/quake/touch_interface.cpp \
                    h2shared/gl_refrag.c \
                  	h2shared/gl_rlight.c \
                  	hexen2/gl_rmain.c \
                  	hexen2/gl_rmisc.c \
                  	hexen2/r_part.c \
                  	hexen2/gl_rsurf.c \
                  	h2shared/gl_screen.c \
                  	h2shared/gl_warp.c \
                  	h2shared/gl_vidsdl.c \
                  	h2shared/gl_draw.c \
                  	h2shared/gl_mesh.c \
                  	hexen2/gl_model.c \
                  	h2shared/in_sdl.c \
                  	h2shared/bgmusic.c \
                    h2shared/snd_codec.c \
                    h2shared/snd_flac.c \
                    h2shared/snd_wave.c \
                    h2shared/snd_vorbis.c \
                    h2shared/snd_opus.c \
                    h2shared/snd_mp3.c \
                    h2shared/snd_mikmod.c \
                    h2shared/snd_xmp.c \
                    h2shared/snd_umx.c \
                    h2shared/snd_timidity.c \
                    h2shared/snd_wildmidi.c \
                    h2shared/snd_sys.c \
                    h2shared/snd_dma.c \
                    h2shared/snd_mix.c \
                    h2shared/snd_mem.c \
                    h2shared/snd_sdl.c \
                    h2shared/cd_null.c \
                    h2shared/midi_nul.c \
                    hexen2/net_bsd.c \
                    hexen2/net_udp.c \
	                hexen2/net_dgrm.c \
	                hexen2/net_loop.c \
	                hexen2/net_main.c \
	                hexen2/chase.c \
	                hexen2/cl_demo.c \
	                hexen2/cl_effect.c \
	                hexen2/cl_inlude.c \
	                hexen2/cl_input.c \
	                hexen2/cl_main.c \
	                hexen2/cl_parse.c \
	                hexen2/cl_string.c \
	                hexen2/cl_tent.c \
	                hexen2/cl_cmd.c \
	                hexen2/console.c \
	                hexen2/keys.c \
	                hexen2/menu.c \
	                hexen2/sbar.c \
	                hexen2/view.c \
	                h2shared/wad.c \
	                h2shared/cmd.c \
	                ../common/q_endian.c \
	                h2shared/link_ops.c \
	                h2shared/sizebuf.c \
	                ../common/strlcat.c \
	                ../common/strlcpy.c \
	                ../common/qsnprint.c \
	                h2shared/msg_io.c \
	                h2shared/common.c \
	                h2shared/debuglog.c \
	                h2shared/quakefs.c \
	                ../common/crc.c \
	                h2shared/cvar.c \
	                h2shared/cfgfile.c \
	                hexen2/host.c \
	                hexen2/host_cmd.c \
	                h2shared/host_string.c \
	                h2shared/mathlib.c \
	                hexen2/pr_cmds.c \
	                hexen2/pr_edict.c \
	                hexen2/pr_exec.c \
	                hexen2/sv_effect.c \
	                hexen2/sv_main.c \
	                hexen2/sv_move.c \
	                hexen2/sv_phys.c \
	                hexen2/sv_user.c \
	                hexen2/world.c \
	                h2shared/zone.c \
                    hexen2/sys_unix.c \
                    h2shared/sys_sdl.c \

LOCAL_LDLIBS := -lEGL -ldl -llog -lOpenSLES -lz -lGLESv1_CM
LOCAL_STATIC_LIBRARIES := sigc libzip libpng logwritter  freetype2-static libjpeg libmad timidity
LOCAL_SHARED_LIBRARIES := touchcontrols SDL2 SDL2_mixer core_shared saffal

include $(BUILD_SHARED_LIBRARY)


