/*	$OpenBSD: stfclock.c,v 1.7 2023/07/07 08:43:47 kettenis Exp $	*/
/*
 * Copyright (c) 2022 Mark Kettenis <kettenis@openbsd.org>
 * Copyright (c) 2023 Joel Sing <jsing@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/ofw_misc.h>
#include <dev/ofw/fdt.h>

/* JH7100 Clock IDs */
#define JH7100_CLK_CPUNDBUS_ROOT	0
#define JH7100_CLK_GMACUSB_ROOT		3
#define JH7100_CLK_PERH0_ROOT		4
#define JH7100_CLK_PERH1_ROOT		5
#define JH7100_CLK_CPUNBUS_ROOT_DIV	12
#define JH7100_CLK_PERH0_SRC		14
#define JH7100_CLK_PERH1_SRC		15
#define JH7100_CLK_PLL2_REF		19
#define JH7100_CLK_AHB_BUS		22
#define JH7100_CLK_SDIO0_AHB		114
#define JH7100_CLK_SDIO0_CCLKINT	115
#define JH7100_CLK_SDIO0_CCLKINT_INV	116
#define JH7100_CLK_SDIO1_AHB		117
#define JH7100_CLK_SDIO1_CCLKINT	118
#define JH7100_CLK_SDIO1_CCLKINT_INV	119
#define JH7100_CLK_GMAC_AHB		120
#define JH7100_CLK_GMAC_ROOT_DIV	121
#define JH7100_CLK_GMAC_GTX		123
#define JH7100_CLK_UART0_CORE		147
#define JH7100_CLK_UART3_CORE		162
#define JH7100_CLK_TEMP_APB		183
#define JH7100_CLK_TEMP_SENSE		184
#define JH7100_CLK_PLL0_OUT		186
#define JH7100_CLK_PLL1_OUT		187
#define JH7100_CLK_PLL2_OUT		188

#define JH7100_CLK_OSC_SYS		255
#define JH7100_CLK_OSC_AUD		254

/* JH7110 Clock IDs */
#define JH7110_AONCLK_GMAC0_AHB		2
#define JH7110_AONCLK_GMAC0_AXI		3
#define JH7110_AONCLK_GMAC0_RMII_RTX	4
#define JH7110_AONCLK_GMAC0_TX		5
#define JH7110_AONCLK_GMAC0_TX_INV	6

#define JH7110_AONCLK_OSC		14
#define JH7110_AONCLK_GMAC0_RMII_REFIN	15
#define JH7110_AONCLK_STG_AXIAHB	17
#define JH7110_AONCLK_GMAC0_GTXCLK	19

#define JH7110_AONCLK_ASSERT_OFFSET	0x38
#define JH7110_AONCLK_STATUS_OFFSET	0x3c

#define JH7110_CLK_PLL0_OUT		0
#define JH7110_CLK_PLL1_OUT		1
#define JH7110_CLK_PLL2_OUT		2

#define JH7110_STGCLK_PCIE0_AXI_MST0	8
#define JH7110_STGCLK_PCIE0_APB		9
#define JH7110_STGCLK_PCIE0_TL		10
#define JH7110_STGCLK_PCIE1_AXI_MST0	11
#define JH7110_STGCLK_PCIE1_APB		12
#define JH7110_STGCLK_PCIE1_TL		13

#define JH7110_STGCLK_ASSERT_OFFSET	0x74
#define JH7110_STGCLK_STATUS_OFFSET	0x78

#define JH7110_SYSCLK_CPU_ROOT		0
#define JH7110_SYSCLK_CPU_CORE		1
#define JH7110_SYSCLK_CPU_BUS		2
#define JH7110_SYSCLK_BUS_ROOT		5
#define JH7110_SYSCLK_AXI_CFG0		7
#define JH7110_SYSCLK_STG_AXIAHB	8
#define JH7110_SYSCLK_AHB0		9
#define JH7110_SYSCLK_AHB1		10
#define JH7110_SYSCLK_APB_BUS		11

#define JH7110_SYSCLK_SDIO0_AHB		91
#define JH7110_SYSCLK_SDIO1_AHB		92
#define JH7110_SYSCLK_SDIO0_SDCARD	93
#define JH7110_SYSCLK_SDIO1_SDCARD	94
#define JH7110_SYSCLK_NOC_BUS_STG_AXI	96
#define JH7110_SYSCLK_GMAC1_AHB		97
#define JH7110_SYSCLK_GMAC1_AXI		98
#define JH7110_SYSCLK_GMAC1_GTXCLK	100
#define JH7110_SYSCLK_GMAC1_RMII_RTX	101
#define JH7110_SYSCLK_GMAC1_PTP		102
#define JH7110_SYSCLK_GMAC1_TX		105
#define JH7110_SYSCLK_GMAC1_TX_INV	106
#define JH7110_SYSCLK_GMAC1_GTXC	107
#define JH7110_SYSCLK_GMAC0_GTXCLK	108
#define JH7110_SYSCLK_GMAC0_PTP		109
#define JH7110_SYSCLK_GMAC0_GTXC	111
#define JH7110_SYSCLK_IOMUX_APB		112
#define JH7110_SYSCLK_TEMP_APB		129
#define JH7110_SYSCLK_TEMP_CORE		130
#define JH7110_SYSCLK_UART0_CORE	146

#define JH7110_SYSCLK_OSC		190
#define JH7110_SYSCLK_GMAC1_RMII_REFIN	191
#define JH7110_SYSCLK_PLL0_OUT		199
#define JH7110_SYSCLK_PLL1_OUT		200
#define JH7110_SYSCLK_PLL2_OUT		201

#define JH7110_SYSCLK_ASSERT_OFFSET	0x2f8
#define JH7110_SYSCLK_STATUS_OFFSET	0x308

/* Registers */
#define CLKMUX_MASK		0x03000000
#define CLKMUX_SHIFT		24
#define CLKDIV_MASK		0x00ffffff
#define CLKDIV_SHIFT		0

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define HSET4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) | (bits))
#define HCLR4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) & ~(bits))

struct stfclock_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	int			sc_node;

	struct clock_device	sc_cd;
	struct reset_device	sc_rd;
};

int	stfclock_match(struct device *, void *, void *);
void	stfclock_attach(struct device *, struct device *, void *);

const struct cfattach stfclock_ca = {
	sizeof (struct stfclock_softc), stfclock_match, stfclock_attach
};

struct cfdriver stfclock_cd = {
	NULL, "stfclock", DV_DULL
};

uint32_t stfclock_get_frequency_jh7100(void *, uint32_t *);
int	stfclock_set_frequency_jh7100(void *, uint32_t *, uint32_t);
void	stfclock_enable_jh7100(void *, uint32_t *, int);

uint32_t stfclock_get_frequency_jh7110_aon(void *, uint32_t *);
int	stfclock_set_frequency_jh7110_aon(void *, uint32_t *, uint32_t);
void	stfclock_enable_jh7110_aon(void *, uint32_t *, int);
void	stfclock_reset_jh7110_aon(void *, uint32_t *, int);

uint32_t stfclock_get_frequency_jh7110_pll(void *, uint32_t *);
int	stfclock_set_frequency_jh7110_pll(void *, uint32_t *, uint32_t);
void	stfclock_enable_jh7110_pll(void *, uint32_t *, int);

uint32_t stfclock_get_frequency_jh7110_stg(void *, uint32_t *);
int	stfclock_set_frequency_jh7110_stg(void *, uint32_t *, uint32_t);
void	stfclock_enable_jh7110_stg(void *, uint32_t *, int);
void	stfclock_reset_jh7110_stg(void *, uint32_t *, int);

uint32_t stfclock_get_frequency_jh7110_sys(void *, uint32_t *);
int	stfclock_set_frequency_jh7110_sys(void *, uint32_t *, uint32_t);
void	stfclock_enable_jh7110_sys(void *, uint32_t *, int);
void	stfclock_reset_jh7110_sys(void *, uint32_t *, int);

int
stfclock_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "starfive,jh7100-clkgen") ||
	    OF_is_compatible(faa->fa_node, "starfive,jh7110-aoncrg") ||
	    OF_is_compatible(faa->fa_node, "starfive,jh7110-pll") ||
	    OF_is_compatible(faa->fa_node, "starfive,jh7110-stgcrg") ||
	    OF_is_compatible(faa->fa_node, "starfive,jh7110-syscrg");
}

void
stfclock_attach(struct device *parent, struct device *self, void *aux)
{
	struct stfclock_softc *sc = (struct stfclock_softc *)self;
	struct fdt_attach_args *faa = aux;

	if (faa->fa_nreg >= 1) {
		sc->sc_iot = faa->fa_iot;
		if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
		    faa->fa_reg[0].size, 0, &sc->sc_ioh)) {
			printf(": can't map registers\n");
			return;
		}
	}

	sc->sc_node = faa->fa_node;

	sc->sc_cd.cd_node = faa->fa_node;
	sc->sc_cd.cd_cookie = sc;

	if (OF_is_compatible(faa->fa_node, "starfive,jh7100-clkgen")) {
		sc->sc_cd.cd_get_frequency = stfclock_get_frequency_jh7100;
		sc->sc_cd.cd_set_frequency = stfclock_set_frequency_jh7100;
		sc->sc_cd.cd_enable = stfclock_enable_jh7100;
		printf("\n");
	} else if (OF_is_compatible(faa->fa_node, "starfive,jh7110-aoncrg")) {
		sc->sc_cd.cd_get_frequency = stfclock_get_frequency_jh7110_aon;
		sc->sc_cd.cd_set_frequency = stfclock_set_frequency_jh7110_aon;
		sc->sc_cd.cd_enable = stfclock_enable_jh7110_aon;

		sc->sc_rd.rd_node = sc->sc_node;
		sc->sc_rd.rd_cookie = sc;
		sc->sc_rd.rd_reset = stfclock_reset_jh7110_aon;
		reset_register(&sc->sc_rd);

		printf(": aoncrg\n");
	} else if (OF_is_compatible(faa->fa_node, "starfive,jh7110-pll")) {
		sc->sc_cd.cd_get_frequency = stfclock_get_frequency_jh7110_pll;
		sc->sc_cd.cd_set_frequency = stfclock_set_frequency_jh7110_pll;
		sc->sc_cd.cd_enable = stfclock_enable_jh7110_pll;
		printf(": pll\n");
	} else if (OF_is_compatible(faa->fa_node, "starfive,jh7110-stgcrg")) {
		sc->sc_cd.cd_get_frequency = stfclock_get_frequency_jh7110_stg;
		sc->sc_cd.cd_set_frequency = stfclock_set_frequency_jh7110_stg;
		sc->sc_cd.cd_enable = stfclock_enable_jh7110_stg;

		sc->sc_rd.rd_node = sc->sc_node;
		sc->sc_rd.rd_cookie = sc;
		sc->sc_rd.rd_reset = stfclock_reset_jh7110_stg;
		reset_register(&sc->sc_rd);

		printf(": stgcrg\n");
	} else if (OF_is_compatible(faa->fa_node, "starfive,jh7110-syscrg")) {
		sc->sc_cd.cd_get_frequency = stfclock_get_frequency_jh7110_sys;
		sc->sc_cd.cd_set_frequency = stfclock_set_frequency_jh7110_sys;
		sc->sc_cd.cd_enable = stfclock_enable_jh7110_sys;

		sc->sc_rd.rd_node = sc->sc_node;
		sc->sc_rd.rd_cookie = sc;
		sc->sc_rd.rd_reset = stfclock_reset_jh7110_sys;
		reset_register(&sc->sc_rd);

		printf(": syscrg\n");
	}

	KASSERT(sc->sc_cd.cd_get_frequency);

	clock_register(&sc->sc_cd);
}

uint32_t
stfclock_get_frequency_jh7100(void *cookie, uint32_t *cells)
{
	struct stfclock_softc *sc = cookie;
	uint32_t idx = cells[0];
	uint32_t parent, freq;
	uint32_t reg, div, mux;

	switch (idx) {
	case JH7100_CLK_OSC_SYS:
		return clock_get_frequency(sc->sc_node, "osc_sys");
	case JH7100_CLK_OSC_AUD:
		return clock_get_frequency(sc->sc_node, "osc_aud");

	case JH7100_CLK_PLL0_OUT:
		parent = JH7100_CLK_OSC_SYS;
		return 40 * stfclock_get_frequency_jh7100(sc, &parent);
	case JH7100_CLK_PLL1_OUT:
		parent = JH7100_CLK_OSC_SYS;
		return 64 * stfclock_get_frequency_jh7100(sc, &parent);
	case JH7100_CLK_PLL2_OUT:
		parent = JH7100_CLK_PLL2_REF;
		return 55 * stfclock_get_frequency_jh7100(sc, &parent);
	}

	reg = HREAD4(sc, idx * 4);
	mux = (reg & CLKMUX_MASK) >> CLKMUX_SHIFT;
	div = (reg & CLKDIV_MASK) >> CLKDIV_SHIFT;

	switch (idx) {
	case JH7100_CLK_CPUNDBUS_ROOT:
		switch (mux) {
		default:
			parent = JH7100_CLK_OSC_SYS;
			break;
		case 1:
			parent = JH7100_CLK_PLL0_OUT;
			break;
		case 2:
			parent = JH7100_CLK_PLL1_OUT;
			break;
		case 3:
			parent = JH7100_CLK_PLL2_OUT;
			break;
		}
		return stfclock_get_frequency_jh7100(sc, &parent);
	case JH7100_CLK_GMACUSB_ROOT:
		switch (mux) {
		default:
			parent = JH7100_CLK_OSC_SYS;
			break;
		case 1:
			parent = JH7100_CLK_PLL0_OUT;
			break;
		case 2:
			parent = JH7100_CLK_PLL2_OUT;
			break;
		}
		return stfclock_get_frequency_jh7100(sc, &parent);	
	case JH7100_CLK_PERH0_ROOT:
		mux = (reg >> 24) & 1;
		parent = mux ? JH7100_CLK_PLL0_OUT : JH7100_CLK_OSC_SYS;
		return stfclock_get_frequency_jh7100(sc, &parent);
	case JH7100_CLK_PERH1_ROOT:
		mux = (reg >> 24) & 1;
		parent = mux ? JH7100_CLK_PLL2_OUT : JH7100_CLK_OSC_SYS;
		return stfclock_get_frequency_jh7100(sc, &parent);
	case JH7100_CLK_PLL2_REF:
		parent = mux ? JH7100_CLK_OSC_AUD : JH7100_CLK_OSC_SYS;
		return stfclock_get_frequency_jh7100(sc, &parent);
	}

	switch (idx) {
	case JH7100_CLK_PERH0_SRC:
		parent = JH7100_CLK_PERH0_ROOT;
		break;
	case JH7100_CLK_PERH1_SRC:
		parent = JH7100_CLK_PERH1_ROOT;
		break;
	case JH7100_CLK_CPUNBUS_ROOT_DIV:
		parent = JH7100_CLK_CPUNDBUS_ROOT;
		break;
	case JH7100_CLK_AHB_BUS:
		parent = JH7100_CLK_CPUNBUS_ROOT_DIV;
		break;
	case JH7100_CLK_SDIO0_CCLKINT:
	case JH7100_CLK_UART3_CORE:
		parent = JH7100_CLK_PERH0_SRC;
		break;
	case JH7100_CLK_SDIO1_CCLKINT:
	case JH7100_CLK_UART0_CORE:
		parent = JH7100_CLK_PERH1_SRC;
		break;
	case JH7100_CLK_SDIO0_AHB:
	case JH7100_CLK_SDIO1_AHB:
	case JH7100_CLK_GMAC_AHB:
		parent = JH7100_CLK_AHB_BUS;
		div = 1;
		break;
	case JH7100_CLK_SDIO0_CCLKINT_INV:
		parent = JH7100_CLK_SDIO0_CCLKINT;
		div = 1;
		break;
	case JH7100_CLK_SDIO1_CCLKINT_INV:
		parent = JH7100_CLK_SDIO1_CCLKINT;
		div = 1;
		break;
	case JH7100_CLK_GMAC_ROOT_DIV:
		parent = JH7100_CLK_GMACUSB_ROOT;
		break;
	case JH7100_CLK_GMAC_GTX:
		parent = JH7100_CLK_GMAC_ROOT_DIV;
		break;
	default:
		printf("%s: unknown clock 0x%08x\n", __func__, idx);
		return 0;
	}

	freq = stfclock_get_frequency_jh7100(sc, &parent);
	return freq / div;
}

int
stfclock_set_frequency_jh7100(void *cookie, uint32_t *cells, uint32_t freq)
{
	uint32_t idx = cells[0];

	printf("%s: not handled 0x%08x (freq=0x%08x)\n", __func__, idx, freq);

	return -1;
}

void
stfclock_enable_jh7100(void *cookie, uint32_t *cells, int on)
{
	struct stfclock_softc *sc = cookie;
	uint32_t idx = cells[0];

	switch (idx) {
	case JH7100_CLK_SDIO0_CCLKINT:
	case JH7100_CLK_SDIO0_CCLKINT_INV:
	case JH7100_CLK_SDIO1_CCLKINT:
	case JH7100_CLK_SDIO1_CCLKINT_INV:
	case JH7100_CLK_SDIO0_AHB:
	case JH7100_CLK_SDIO1_AHB:
	case JH7100_CLK_GMAC_AHB:
	case JH7100_CLK_GMAC_GTX:
	case JH7100_CLK_UART0_CORE:
	case JH7100_CLK_UART3_CORE:
	case JH7100_CLK_TEMP_APB:
	case JH7100_CLK_TEMP_SENSE:
		if (on)
			HSET4(sc, idx * 4, 1U << 31);
		else
			HCLR4(sc, idx * 4, 1U << 31);
		return;
	case JH7100_CLK_GMAC_ROOT_DIV:
		/* No gate */
		return;
	}

	printf("%s: unknown clock 0x%08x\n", __func__, idx);
}

uint32_t
stfclock_get_frequency_jh7110_aon(void *cookie, uint32_t *cells)
{
	struct stfclock_softc *sc = cookie;
	uint32_t idx = cells[0];
	uint32_t parent, freq;
	uint32_t reg, div, mux;

	switch (idx) {
	case JH7110_AONCLK_OSC:
		return clock_get_frequency(sc->sc_node, "osc");
	case JH7110_AONCLK_STG_AXIAHB:
		return clock_get_frequency(sc->sc_node, "stg_axiahb");
	case JH7110_AONCLK_GMAC0_RMII_REFIN:
		return clock_get_frequency(sc->sc_node, "gmac0_rmii_refin");
	}

	reg = HREAD4(sc, idx * 4);
	mux = (reg & CLKMUX_MASK) >> CLKMUX_SHIFT;
	div = (reg & CLKDIV_MASK) >> CLKDIV_SHIFT;

	switch (idx) {
	case JH7110_AONCLK_GMAC0_TX:
		parent = mux ? JH7110_AONCLK_GMAC0_RMII_RTX:
		    JH7110_AONCLK_GMAC0_GTXCLK;
		return stfclock_get_frequency_jh7110_aon(sc, &parent);
	}

	switch (idx) {
	case JH7110_AONCLK_GMAC0_AXI:
		parent = JH7110_AONCLK_STG_AXIAHB;
		div = 1;
		break;
	case JH7110_AONCLK_GMAC0_RMII_RTX:
		parent = JH7110_AONCLK_GMAC0_RMII_REFIN;
		break;
	case JH7110_AONCLK_GMAC0_TX_INV:
		parent = JH7110_AONCLK_GMAC0_TX;
		div = 1;
		break;
	default:
		printf("%s: unknown clock 0x%08x\n", __func__, idx);
		return 0;
	}

	if (div == 0) {
		printf("%s: zero divisor for clock 0x%08x\n", __func__, idx);
		return 0;
	}

	freq = stfclock_get_frequency_jh7110_aon(sc, &parent);
	return freq / div;
}

int
stfclock_set_frequency_jh7110_aon(void *cookie, uint32_t *cells, uint32_t freq)
{
	uint32_t idx = cells[0];

	printf("%s: not handled 0x%08x (freq=0x%08x)\n", __func__, idx, freq);

	return -1;
}

void
stfclock_enable_jh7110_aon(void *cookie, uint32_t *cells, int on)
{
	struct stfclock_softc *sc = cookie;
	uint32_t idx = cells[0];

	switch (idx) {
	case JH7110_AONCLK_GMAC0_TX_INV:
		idx = JH7110_AONCLK_GMAC0_TX;
		break;
	}

	switch (idx) {
	case JH7110_AONCLK_GMAC0_AHB:
	case JH7110_AONCLK_GMAC0_AXI:
	case JH7110_AONCLK_GMAC0_TX:
		if (on)
			HSET4(sc, idx * 4, 1U << 31);
		else
			HCLR4(sc, idx * 4, 1U << 31);
		return;
	}

	printf("%s: unknown clock 0x%08x\n", __func__, idx);
}

void
stfclock_reset_jh7110_aon(void *cookie, uint32_t *cells, int assert)
{
	struct stfclock_softc *sc = cookie;
	uint32_t idx = cells[0];
	uint32_t bits, offset;

	offset = JH7110_AONCLK_ASSERT_OFFSET + (idx / 32) * 4;
	bits = 1U << (idx % 32);

	if (assert)
		HSET4(sc, offset, bits);
	else
		HCLR4(sc, offset, bits);
}

uint32_t
stfclock_get_frequency_jh7110_pll(void *cookie, uint32_t *cells)
{
	struct stfclock_softc *sc = cookie;
	uint32_t idx = cells[0];
	uint32_t dacpd, dsmpd, fbdiv, frac, prediv, postdiv1, reg;
	uint64_t frac_val, parent_freq;
	struct regmap *pllrm;
	int syscon_node;
	bus_size_t base;

	parent_freq = clock_get_frequency_idx(sc->sc_node, 0);
	if (parent_freq == 0) {
		printf("%s: failed to get parent frequency\n", __func__);
		return 0;
	}

	switch (idx) {
	case JH7110_CLK_PLL0_OUT:
		base = 0x18;
		break;
	case JH7110_CLK_PLL1_OUT:
		base = 0x24;
		break;
	case JH7110_CLK_PLL2_OUT:
		base = 0x2c;
		break;
	default:
		printf("%s: unknown clock 0x08%x\n", __func__, idx);
		return 0;
	}

	syscon_node = OF_parent(sc->sc_node);
	if (syscon_node < 0) {
		printf("%s: failed to find pll clock parent\n", __func__);
		return 0;
	}
	pllrm = regmap_bynode(syscon_node);
	if (pllrm == NULL) {
		printf("%s: failed to get pll regmap\n", __func__);
		return 0;
	}

	switch (idx) {
	case JH7110_CLK_PLL0_OUT:
		reg = regmap_read_4(pllrm, base + 0);
		dacpd = (reg >> 24) & 1;
		dsmpd = (reg >> 25) & 1;

		reg = regmap_read_4(pllrm, base + 4);
		fbdiv = (reg >> 0) & ((1UL << 12) - 1);
	
		reg = regmap_read_4(pllrm, base + 8);
		frac = (reg >> 0) & ((1UL << 24) - 1);
		postdiv1 = 1 << ((reg >> 28) & ((1UL << 2) - 1));

		reg = regmap_read_4(pllrm, base + 12);
		prediv = (reg >> 0) & ((1UL << 6) - 1);
		break;

	case JH7110_CLK_PLL1_OUT:
	case JH7110_CLK_PLL2_OUT:
		reg = regmap_read_4(pllrm, base + 0);
		dacpd = (reg >> 15) & 1;
		dsmpd = (reg >> 16) & 1;
		fbdiv = (reg >> 17) & ((1UL << 12) - 1);

		reg = regmap_read_4(pllrm, base + 4);
		frac = (reg >> 0) & ((1UL << 24) - 1);
		postdiv1 = 1 << ((reg >> 28) & ((1UL << 2) - 1));
		
		reg = regmap_read_4(pllrm, base + 8);
		prediv = (reg >> 0) & ((1UL << 6) - 1);
		break;
	}

	if (fbdiv == 0 || prediv == 0 || postdiv1 == 0) {
		printf("%s: zero divisor\n", __func__);
		return 0;
	}

	if (dacpd != dsmpd)
		return 0;

	/* Integer mode (dacpd/dsmpd both 0) or fraction mode (both 1). */
	frac_val = 0;
	if (dacpd == 0 && dsmpd == 0)
		frac_val = ((uint64_t)frac * 1000) / (1 << 24);

	return parent_freq / 1000 * (fbdiv * 1000 + frac_val) / prediv / postdiv1;
}

int
stfclock_set_frequency_jh7110_pll(void *cookie, uint32_t *cells, uint32_t freq)
{
	uint32_t idx = cells[0];

	printf("%s: not handled 0x%08x (freq=0x%08x)\n", __func__, idx, freq);

	return -1;
}

void
stfclock_enable_jh7110_pll(void *cookie, uint32_t *cells, int on)
{
	uint32_t idx = cells[0];

	printf("%s: not handled 0x%08x\n", __func__, idx);
}

uint32_t
stfclock_get_frequency_jh7110_stg(void *cookie, uint32_t *cells)
{
	uint32_t idx = cells[0];

	printf("%s: unknown clock 0x%08x\n", __func__, idx);
	return 0;
}

int
stfclock_set_frequency_jh7110_stg(void *cookie, uint32_t *cells, uint32_t freq)
{
	uint32_t idx = cells[0];

	printf("%s: not handled 0x%08x (freq=0x%08x)\n", __func__, idx, freq);

	return -1;
}

void
stfclock_enable_jh7110_stg(void *cookie, uint32_t *cells, int on)
{
	struct stfclock_softc *sc = cookie;
	uint32_t idx = cells[0];

	switch (idx) {
	case JH7110_STGCLK_PCIE0_AXI_MST0:
	case JH7110_STGCLK_PCIE0_APB:
	case JH7110_STGCLK_PCIE0_TL:
	case JH7110_STGCLK_PCIE1_AXI_MST0:
	case JH7110_STGCLK_PCIE1_APB:
	case JH7110_STGCLK_PCIE1_TL:
		if (on)
			HSET4(sc, idx * 4, 1U << 31);
		else
			HCLR4(sc, idx * 4, 1U << 31);
		return;
	}

	printf("%s: unknown clock 0x%08x\n", __func__, idx);
}

void
stfclock_reset_jh7110_stg(void *cookie, uint32_t *cells, int assert)
{
	struct stfclock_softc *sc = cookie;
	uint32_t idx = cells[0];
	uint32_t bits, offset;

	offset = JH7110_STGCLK_ASSERT_OFFSET + (idx / 32) * 4;
	bits = 1U << (idx % 32);

	if (assert)
		HSET4(sc, offset, bits);
	else
		HCLR4(sc, offset, bits);
}

uint32_t
stfclock_get_frequency_jh7110_sys(void *cookie, uint32_t *cells)
{
	struct stfclock_softc *sc = cookie;
	uint32_t idx = cells[0];
	uint32_t parent, freq;
	uint32_t reg, div, mux;

	switch (idx) {
	case JH7110_SYSCLK_OSC:
		return clock_get_frequency(sc->sc_node, "osc");
	case JH7110_SYSCLK_PLL0_OUT:
		return clock_get_frequency(sc->sc_node, "pll0_out");
	case JH7110_SYSCLK_PLL1_OUT:
		return clock_get_frequency(sc->sc_node, "pll1_out");
	case JH7110_SYSCLK_PLL2_OUT:
		return clock_get_frequency(sc->sc_node, "pll2_out");
	}

	reg = HREAD4(sc, idx * 4);
	mux = (reg & CLKMUX_MASK) >> CLKMUX_SHIFT;
	div = (reg & CLKDIV_MASK) >> CLKDIV_SHIFT;

	switch (idx) {
	case JH7110_SYSCLK_CPU_ROOT:
		mux = (reg >> 24) & 1;
		parent = mux ? JH7110_SYSCLK_PLL0_OUT : JH7110_SYSCLK_OSC;
		return stfclock_get_frequency_jh7110_sys(sc, &parent);
	case JH7110_SYSCLK_BUS_ROOT:
		mux = (reg >> 24) & 1;
		parent = mux ? JH7110_SYSCLK_PLL2_OUT : JH7110_SYSCLK_OSC;
		return stfclock_get_frequency_jh7110_sys(sc, &parent);
	}

	switch (idx) {
	case JH7110_SYSCLK_CPU_CORE:
		parent = JH7110_SYSCLK_CPU_ROOT;
		break;
	case JH7110_SYSCLK_AXI_CFG0:
		parent = JH7110_SYSCLK_BUS_ROOT;
		break;
	case JH7110_SYSCLK_STG_AXIAHB:
		parent = JH7110_SYSCLK_AXI_CFG0;
		break;
	case JH7110_SYSCLK_AHB0:
	case JH7110_SYSCLK_AHB1:
	case JH7110_SYSCLK_APB_BUS:
		parent = JH7110_SYSCLK_STG_AXIAHB;
		break;
	case JH7110_SYSCLK_SDIO0_AHB:
	case JH7110_SYSCLK_SDIO1_AHB:
		parent = JH7110_SYSCLK_AHB0;
		break;
	case JH7110_SYSCLK_SDIO0_SDCARD:
	case JH7110_SYSCLK_SDIO1_SDCARD:
		parent = JH7110_SYSCLK_AXI_CFG0;
		break;
	case JH7110_SYSCLK_GMAC1_AXI:
		parent = JH7110_SYSCLK_STG_AXIAHB;
		div = 1;
		break;
	case JH7110_SYSCLK_GMAC1_GTXCLK:
		parent = JH7110_SYSCLK_PLL0_OUT;
		break;
	case JH7110_SYSCLK_GMAC1_RMII_RTX:
		parent = JH7110_SYSCLK_GMAC1_RMII_REFIN;
		break;
	case JH7110_SYSCLK_GMAC1_TX_INV:
		parent = JH7110_SYSCLK_GMAC1_TX;
		div = 1;
		break;
	case JH7110_SYSCLK_GMAC0_GTXCLK:
		parent = JH7110_SYSCLK_PLL0_OUT;
		break;
	case JH7110_SYSCLK_TEMP_APB:
		parent = JH7110_SYSCLK_APB_BUS;
		break;
	case JH7110_SYSCLK_TEMP_CORE:
		parent = JH7110_SYSCLK_OSC;
		break;
	case JH7110_SYSCLK_UART0_CORE:
		parent = JH7110_SYSCLK_OSC;
		div = 1;
		break;
	default:
		printf("%s: unknown clock 0x%08x\n", __func__, idx);
		return 0;
	}

	if (div == 0) {
		printf("%s: zero divisor for clock 0x%08x\n", __func__, idx);
		return 0;
	}

	freq = stfclock_get_frequency_jh7110_sys(sc, &parent);
	return freq / div;
}

int
stfclock_set_frequency_jh7110_sys(void *cookie, uint32_t *cells, uint32_t freq)
{
	uint32_t idx = cells[0];

	printf("%s: not handled 0x%08x (freq=0x%08x)\n", __func__, idx, freq);

	return -1;
}

void
stfclock_enable_jh7110_sys(void *cookie, uint32_t *cells, int on)
{
	struct stfclock_softc *sc = cookie;
	uint32_t idx = cells[0];
	uint32_t parent;

	switch (idx) {
	case JH7110_SYSCLK_GMAC1_TX_INV:
		idx = JH7110_SYSCLK_GMAC1_TX;
		break;
	case JH7110_SYSCLK_GMAC1_GTXC:
		parent = JH7110_SYSCLK_GMAC1_GTXCLK;
		stfclock_enable_jh7110_sys(sc, &parent, on);
		break;
	case JH7110_SYSCLK_GMAC0_GTXC:
		parent = JH7110_SYSCLK_GMAC0_GTXCLK;
		stfclock_enable_jh7110_sys(sc, &parent, on);
		break;
	}

	switch (idx) {
	case JH7110_SYSCLK_SDIO0_AHB:
	case JH7110_SYSCLK_SDIO1_AHB:
	case JH7110_SYSCLK_SDIO0_SDCARD:
	case JH7110_SYSCLK_SDIO1_SDCARD:
	case JH7110_SYSCLK_NOC_BUS_STG_AXI:
	case JH7110_SYSCLK_GMAC1_AHB:
	case JH7110_SYSCLK_GMAC1_AXI:
	case JH7110_SYSCLK_GMAC1_GTXCLK:
	case JH7110_SYSCLK_GMAC1_PTP:
	case JH7110_SYSCLK_GMAC1_TX:
	case JH7110_SYSCLK_GMAC1_GTXC:
	case JH7110_SYSCLK_GMAC0_GTXCLK:
	case JH7110_SYSCLK_GMAC0_PTP:
	case JH7110_SYSCLK_GMAC0_GTXC:
	case JH7110_SYSCLK_IOMUX_APB:
	case JH7110_SYSCLK_TEMP_APB:
	case JH7110_SYSCLK_TEMP_CORE:
	case JH7110_SYSCLK_UART0_CORE:
		if (on)
			HSET4(sc, idx * 4, 1U << 31);
		else
			HCLR4(sc, idx * 4, 1U << 31);
		return;
	}

	printf("%s: unknown clock 0x%08x\n", __func__, idx);
}

void
stfclock_reset_jh7110_sys(void *cookie, uint32_t *cells, int assert)
{
	struct stfclock_softc *sc = cookie;
	uint32_t idx = cells[0];
	uint32_t bits, offset;

	offset = JH7110_SYSCLK_ASSERT_OFFSET + (idx / 32) * 4;
	bits = 1U << (idx % 32);

	if (assert)
		HSET4(sc, offset, bits);
	else
		HCLR4(sc, offset, bits);
}
