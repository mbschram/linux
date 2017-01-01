/*
 * Page table types definitions.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __ASM_PGTABLE_TYPES_H
#define __ASM_PGTABLE_TYPES_H

#if CONFIG_PGTABLE_LEVELS == 2
#include <asm/pgtable-2level-types.h>
#elif CONFIG_PGTABLE_LEVELS == 3
#include <asm/pgtable-3level-types.h>
#endif

#endif	/* __ASM_PGTABLE_TYPES_H */
