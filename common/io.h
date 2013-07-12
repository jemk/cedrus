/*
 * Copyright (c) 2013 Jens Kuske <jenskuske@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#ifndef __IO_H__
#define __IO_H__

#include <stdint.h>

static inline void writeb(void *addr, uint8_t val)
{
	*((volatile uint8_t *)addr) = val;
}

static inline void writel(void *addr, uint32_t val)
{
	*((volatile uint32_t *)addr) = val;
}

static inline uint8_t readb(void *addr)
{
	return *((volatile uint8_t *) addr);
}

static inline uint32_t readl(void *addr)
{
	return *((volatile uint32_t *) addr);
}

#endif
