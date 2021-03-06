// license:BSD-3-Clause
// copyright-holders:Tomasz Slanina
/*
 Super Othello (c)1986 Fujiwara/Success

    driver by Tomasz Slanina

         1    2    3    4     5     6     7      8      9     10     11    12
+---------------------------------------------------------------------------------+
|                                                                                 |
+-+    LA460  LA6324  M5205  X3   Z80A    1      2    5816P  74374  74138         |  A
  |                                                                               |
  |                                                                               |
+-+        74367 Y3014 74174 74174       7404   7474  74138  7404   7432          |  B
|                                                                                 |
|                                                                                 |
|            74367 DSW1 YM2203    Z80A    3      4      5           6264          |  C
| J                                                                               |
| A                                                                               |
| M   C1663 74367  DSW2           7408 74125   7404  74138   74139  74174  7408   |  D
| M           X2       7414  7474                                                 |
| A                                                                               |
|     C1663 V9938 41464 41464       X1   7474  74139  7432   74157  74244  7432   |  E
|                                                                                 |
|                                                                                 |
+-+   C1663       41464 41464     6809B   6     6264   6264  6264   74244  74245  |  F
  |                                                                               |
  |                                                                               |
+-+   C1663                                                                       |  H
|                                                                                 |
+---------------------------------------------------------------------------------+

CPU  : Z80A(x2) HD68B09P
Sound: YM2203?(surface scratched) + M5205
OSC  : 8.0000MHz(X1)   21.477 MHz(X2)   384kHz(X3)

*/

#include "emu.h"
#include "cpu/m6809/m6809.h"
#include "cpu/z80/z80.h"
#include "machine/gen_latch.h"
#include "sound/2203intf.h"
#include "sound/msm5205.h"
#include "video/v9938.h"
#include "speaker.h"


class sothello_state : public driver_device
{
public:
	sothello_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		m_soundcpu(*this, "soundcpu"),
		m_subcpu(*this, "subcpu"),
		m_v9938(*this, "v9938"),
		m_msm(*this, "msm"),
		m_mainbank(*this, "mainbank")
	{ }

	DECLARE_WRITE8_MEMBER(bank_w);
	DECLARE_READ8_MEMBER(subcpu_halt_set);
	DECLARE_READ8_MEMBER(subcpu_halt_clear);
	DECLARE_READ8_MEMBER(subcpu_comm_status);
	DECLARE_READ8_MEMBER(soundcpu_status_r);
	DECLARE_WRITE8_MEMBER(msm_data_w);
	DECLARE_WRITE8_MEMBER(soundcpu_busyflag_set_w);
	DECLARE_WRITE8_MEMBER(soundcpu_busyflag_reset_w);
	DECLARE_WRITE8_MEMBER(soundcpu_int_clear_w);
	DECLARE_WRITE8_MEMBER(subcpu_status_w);
	DECLARE_READ8_MEMBER(subcpu_status_r);
	DECLARE_WRITE8_MEMBER(msm_cfg_w);
	DECLARE_WRITE_LINE_MEMBER(adpcm_int);

	void sothello(machine_config &config);
	void maincpu_io_map(address_map &map);
	void maincpu_mem_map(address_map &map);
	void soundcpu_io_map(address_map &map);
	void soundcpu_mem_map(address_map &map);
	void subcpu_mem_map(address_map &map);
protected:
	virtual void machine_start() override;
	virtual void machine_reset() override;

private:
	int m_subcpu_status;
	int m_soundcpu_busy;
	int m_msm_data;

	TIMER_CALLBACK_MEMBER(subcpu_suspend);
	TIMER_CALLBACK_MEMBER(subcpu_resume);
	void unlock_shared_ram();

	required_device<cpu_device> m_maincpu;
	required_device<cpu_device> m_soundcpu;
	required_device<cpu_device> m_subcpu;
	required_device<v9938_device> m_v9938;
	required_device<msm5205_device> m_msm;
	required_memory_bank m_mainbank;
};


#define VDP_MEM             0x40000




/* main Z80 */

void sothello_state::machine_start()
{
	m_mainbank->configure_entries(0, 4, memregion("maincpu")->base() + 0x8000, 0x4000);

	save_item(NAME(m_subcpu_status));
	save_item(NAME(m_soundcpu_busy));
	save_item(NAME(m_msm_data));
}

WRITE8_MEMBER(sothello_state::bank_w)
{
	int bank=0;
	switch(data^0xff)
	{
		case 1: bank=0; break;
		case 2: bank=1; break;
		case 4: bank=2; break;
		case 8: bank=3; break;
	}
	m_mainbank->set_entry(bank);
}

TIMER_CALLBACK_MEMBER(sothello_state::subcpu_suspend)
{
	m_subcpu->suspend(SUSPEND_REASON_HALT, 1);
}

TIMER_CALLBACK_MEMBER(sothello_state::subcpu_resume)
{
	m_subcpu->resume(SUSPEND_REASON_HALT);
	m_subcpu->set_input_line(INPUT_LINE_NMI, PULSE_LINE);
}

READ8_MEMBER(sothello_state::subcpu_halt_set)
{
	machine().scheduler().synchronize(timer_expired_delegate(FUNC(sothello_state::subcpu_suspend), this));
	m_subcpu_status|=2;
	return 0;
}

READ8_MEMBER(sothello_state::subcpu_halt_clear)
{
	machine().scheduler().synchronize(timer_expired_delegate(FUNC(sothello_state::subcpu_resume), this));
	m_subcpu_status&=~1;
	m_subcpu_status&=~2;
	return 0;
}

READ8_MEMBER(sothello_state::subcpu_comm_status)
{
	return m_subcpu_status;
}

READ8_MEMBER(sothello_state::soundcpu_status_r)
{
	return m_soundcpu_busy;
}

void sothello_state::maincpu_mem_map(address_map &map)
{
	map(0x0000, 0x7fff).rom().region("maincpu", 0);
	map(0x8000, 0xbfff).bankr("mainbank");
	map(0xc000, 0xc7ff).ram().mirror(0x1800).share("mainsub");
	map(0xe000, 0xffff).ram();
}

void sothello_state::maincpu_io_map(address_map &map)
{
	map.global_mask(0xff);
	map(0x00, 0x0f).portr("INPUT1");
	map(0x10, 0x1f).portr("INPUT2");
	map(0x20, 0x2f).portr("SYSTEM");
	map(0x30, 0x30).r(this, FUNC(sothello_state::subcpu_halt_set));
	map(0x31, 0x31).r(this, FUNC(sothello_state::subcpu_halt_clear));
	map(0x32, 0x32).r(this, FUNC(sothello_state::subcpu_comm_status));
	map(0x33, 0x33).r(this, FUNC(sothello_state::soundcpu_status_r));
	map(0x40, 0x4f).w("soundlatch", FUNC(generic_latch_8_device::write));
	map(0x50, 0x50).w(this, FUNC(sothello_state::bank_w));
	map(0x60, 0x61).mirror(0x02).rw("ymsnd", FUNC(ym2203_device::read), FUNC(ym2203_device::write));
						/* not sure, but the A1 line is ignored, code @ $8b8 */
	map(0x70, 0x73).rw(m_v9938, FUNC(v9938_device::read), FUNC(v9938_device::write));
}

/* sound Z80 */

WRITE8_MEMBER(sothello_state::msm_cfg_w)
{
/*
     bit 0 = RESET
     bit 1 = 4B/3B 0
     bit 2 = S2    1
     bit 3 = S1    2
*/
	m_msm->playmode_w(bitswap<8>((data>>1), 7,6,5,4,3,0,1,2));
	m_msm->reset_w(data & 1);
}

WRITE8_MEMBER(sothello_state::msm_data_w)
{
	m_msm_data = data;
}

WRITE8_MEMBER(sothello_state::soundcpu_busyflag_set_w)
{
	m_soundcpu_busy=1;
}

WRITE8_MEMBER(sothello_state::soundcpu_busyflag_reset_w)
{
	m_soundcpu_busy=0;
}

WRITE8_MEMBER(sothello_state::soundcpu_int_clear_w)
{
	m_soundcpu->set_input_line(0, CLEAR_LINE);
}

void sothello_state::soundcpu_mem_map(address_map &map)
{
	map(0x0000, 0xdfff).rom().region("soundcpu", 0);
	map(0xf800, 0xffff).ram();
}

void sothello_state::soundcpu_io_map(address_map &map)
{
	map.global_mask(0xff);
	map(0x00, 0x00).r("soundlatch", FUNC(generic_latch_8_device::read));
	map(0x01, 0x01).w(this, FUNC(sothello_state::msm_data_w));
	map(0x02, 0x02).w(this, FUNC(sothello_state::msm_cfg_w));
	map(0x03, 0x03).w(this, FUNC(sothello_state::soundcpu_busyflag_set_w));
	map(0x04, 0x04).w(this, FUNC(sothello_state::soundcpu_busyflag_reset_w));
	map(0x05, 0x05).w(this, FUNC(sothello_state::soundcpu_int_clear_w));
}

/* sub 6809 */

void sothello_state::unlock_shared_ram()
{
	if(!m_subcpu->suspended(SUSPEND_REASON_HALT))
	{
		m_subcpu_status|=1;
	}
	else
	{
		//logerror("%s Sub cpu active!\n",machine().describe_context());
	}
}

WRITE8_MEMBER(sothello_state::subcpu_status_w)
{
	unlock_shared_ram();
}

READ8_MEMBER(sothello_state::subcpu_status_r)
{
	unlock_shared_ram();
	return 0;
}

void sothello_state::subcpu_mem_map(address_map &map)
{
	map(0x0000, 0x1fff).rw(this, FUNC(sothello_state::subcpu_status_r), FUNC(sothello_state::subcpu_status_w));
	map(0x2000, 0x77ff).ram();
	map(0x7800, 0x7fff).ram().share("mainsub");  /* upper 0x800 of 6264 is shared with main cpu */
	map(0x8000, 0xffff).rom().region("subcpu", 0);
}

static INPUT_PORTS_START( sothello )
	PORT_START("INPUT1")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_8WAY PORT_PLAYER(1)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_8WAY PORT_PLAYER(1)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_8WAY PORT_PLAYER(1)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_8WAY PORT_PLAYER(1)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(1)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(1)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_PLAYER(1)
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON4 ) PORT_PLAYER(1)

	PORT_START("INPUT2")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_8WAY PORT_PLAYER(2)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_8WAY PORT_PLAYER(2)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_8WAY PORT_PLAYER(2)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_8WAY PORT_PLAYER(2)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(2)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(2)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_PLAYER(2)
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON4 ) PORT_PLAYER(2)

	PORT_START("SYSTEM")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0xf2, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("DSWA")
	PORT_DIPNAME( 0xe0, 0xe0, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_4C ) )
	PORT_DIPNAME( 0x1c, 0x10, "Timer" )
	PORT_DIPSETTING(    0x1c, "15" )
	PORT_DIPSETTING(    0x18, "20" )
	PORT_DIPSETTING(    0x14, "25" )
	PORT_DIPSETTING(    0x10, "30" )
	PORT_DIPSETTING(    0x0c, "35" )
	PORT_DIPSETTING(    0x08, "40" )
	PORT_DIPSETTING(    0x04, "45" )
	PORT_DIPSETTING(    0x00, "50" )
	PORT_BIT( 0x03, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("DSWB")
	PORT_DIPNAME( 0xc0, 0x80, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Very_Hard ) )
	PORT_DIPNAME( 0x30, 0x10, "Matta" ) /* undo moves */
	PORT_DIPSETTING(    0x30, "0" )
	PORT_DIPSETTING(    0x20, "1" )
	PORT_DIPSETTING(    0x10, "2" )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPNAME( 0x08, 0x08, "Games for 2 players" )
	PORT_DIPSETTING(    0x08, "1" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_BIT( 0x07, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END


WRITE_LINE_MEMBER(sothello_state::adpcm_int)
{
	/* only 4 bits are used */
	m_msm->data_w(m_msm_data & 0x0f);
	m_soundcpu->set_input_line(0, ASSERT_LINE);
}

void sothello_state::machine_reset()
{
	m_subcpu_status = 0;
	m_soundcpu_busy = 0;
	m_msm_data = 0;
}

MACHINE_CONFIG_START(sothello_state::sothello)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", Z80, XTAL(21'477'272) / 6)
	MCFG_CPU_PROGRAM_MAP(maincpu_mem_map)
	MCFG_CPU_IO_MAP(maincpu_io_map)

	MCFG_CPU_ADD("soundcpu", Z80, XTAL(21'477'272) / 6)
	MCFG_CPU_PROGRAM_MAP(soundcpu_mem_map)
	MCFG_CPU_IO_MAP(soundcpu_io_map)

	MCFG_CPU_ADD("subcpu", MC6809, XTAL(8'000'000)) // divided by 4 internally
	MCFG_CPU_PROGRAM_MAP(subcpu_mem_map)

	MCFG_QUANTUM_TIME(attotime::from_hz(600))

	/* video hardware */
	MCFG_V9938_ADD("v9938", "screen", VDP_MEM, XTAL(21'477'272))
	MCFG_V99X8_INTERRUPT_CALLBACK(INPUTLINE("maincpu", 0))
	MCFG_V99X8_SCREEN_ADD_NTSC("screen", "v9938", XTAL(21'477'272))

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")

	MCFG_GENERIC_LATCH_8_ADD("soundlatch")

	MCFG_SOUND_ADD("ymsnd", YM2203, XTAL(21'477'272) / 12)
	MCFG_YM2203_IRQ_HANDLER(INPUTLINE("subcpu", 0))
	MCFG_AY8910_PORT_A_READ_CB(IOPORT("DSWA"))
	MCFG_AY8910_PORT_B_READ_CB(IOPORT("DSWB"))
	MCFG_SOUND_ROUTE(0, "mono", 0.25)
	MCFG_SOUND_ROUTE(1, "mono", 0.25)
	MCFG_SOUND_ROUTE(2, "mono", 0.25)
	MCFG_SOUND_ROUTE(3, "mono", 0.50)

	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.30)

	MCFG_SOUND_ADD("msm", MSM5205, XTAL(384'000))
	MCFG_MSM5205_VCLK_CB(WRITELINE(sothello_state, adpcm_int))      /* interrupt function */
	MCFG_MSM5205_PRESCALER_SELECTOR(S48_4B)  /* changed on the fly */
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 1.0)
MACHINE_CONFIG_END

/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( sothello )
	ROM_REGION( 0x18000, "maincpu", 0 )
	ROM_LOAD( "3.7c",   0x00000, 0x8000, CRC(47f97bd4) SHA1(52c9638f098fdcf66903fad7dafe3ab171758572) )
	ROM_LOAD( "4.8c",   0x08000, 0x8000, CRC(a98414e9) SHA1(6d14e1f9c79b95101e0aa101034f398af09d7f32) )
	ROM_LOAD( "5.9c",   0x10000, 0x8000, CRC(e5b5d61e) SHA1(2e4b3d85f41d0796a4d61eae40dd824769e1db86) )

	ROM_REGION( 0x10000, "soundcpu", 0 )
	ROM_LOAD( "1.7a",   0x0000, 0x8000, CRC(6951536a) SHA1(64d07a692d6a167334c825dc173630b02584fdf6) )
	ROM_LOAD( "2.8a",   0x8000, 0x8000, CRC(9c535317) SHA1(b2e69b489e111d6f8105e68fade6e5abefb825f7) )

	ROM_REGION( 0x8000, "subcpu", 0 )
	ROM_LOAD( "6.7f",   0x0000, 0x8000, CRC(ee80fc78) SHA1(9a9d7925847d7a36930f0761c70f67a9affc5e7c) )
ROM_END

GAME( 1986, sothello,  0,       sothello,  sothello, sothello_state,  0, ROT0, "Success / Fujiwara", "Super Othello", 0 )
