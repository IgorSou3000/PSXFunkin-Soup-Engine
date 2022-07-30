TARGET = funkin
TYPE = ps-exe

SRCS = src/main.c \
       src/engine/mutil.c \
       src/engine/random.c \
       src/engine/archive.c \
       src/font/font.c \
       src/engine/trans.c \
       src/engine/loadscr.c \
       src/engine/save.c \
       src/stage.c \
       src/debug.c \
       src/psx.c \
       src/engine/io.c \
       src/engine/gfx.c \
       src/engine/audio.c \
       src/engine/pad.c \
       src/engine/timer.c \
       src/engine/movie.c \
       src/engine/animation.c \
       src/engine/character.c \
       \
       \
       src/stage/dummy.c \
       src/stage/week1.c \
       src/stage/week2.c \
       src/stage/week3.c \
       src/stage/week4.c \
       src/stage/week5.c \
       src/stage/week6.c \
       \
       \
       src/menu/menu.c \
       src/menu/menuplayer.c \
       src/menu/menuopponent.c \
       \
       \
       src/character/bf.c \
       src/character/bfweeb.c \
       src/character/speaker.c \
       src/character/dad.c \
       src/character/spook.c \
       src/character/monster.c \
       src/character/monsterx.c \
       src/character/pico.c \
       src/character/mom.c \
       src/character/xmasbf.c \
       src/character/xmasgf.c \
       src/character/xmasp.c \
       src/character/senpai.c \
       src/character/senpaim.c \
       src/character/spirit.c \
       src/character/gf.c \
       src/character/gfweeb.c \
       src/character/clucky.c \
       \
       \
       src/object/object.c \
       src/object/combo.c \
       src/object/splash.c \
       mips/common/crt0/crt0.s

CPPFLAGS += -Wall -Wextra -pedantic -mno-check-zero-division
LDFLAGS += -Wl,--start-group
# TODO: remove unused libraries
LDFLAGS += -lapi
#LDFLAGS += -lc
LDFLAGS += -lc2
LDFLAGS += -lcard
LDFLAGS += -lcd
#LDFLAGS += -lcomb
LDFLAGS += -lds
LDFLAGS += -letc
LDFLAGS += -lgpu
#LDFLAGS += -lgs
#LDFLAGS += -lgte
#LDFLAGS += -lgun
#LDFLAGS += -lhmd
#LDFLAGS += -lmath
#LDFLAGS += -lmcrd
#LDFLAGS += -lmcx
LDFLAGS += -lpad
LDFLAGS += -lpress
#LDFLAGS += -lsio
LDFLAGS += -lsnd
LDFLAGS += -lspu
#LDFLAGS += -ltap
LDFLAGS += -flto -Wl,--end-group

include mips/common.mk
