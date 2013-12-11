/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "cm_local.h"

/*
 * @brief If true, BSP area culling is skipped.
 */
_Bool cm_no_areas;

/*
 * @brief
 */
static void Cm_DecompressVis(const byte *in, byte *out) {
	int32_t c;
	byte *out_p;
	int32_t row;

	row = (cm_vis->num_clusters + 7) >> 3;
	out_p = out;

	if (!in || !cm_bsp.num_visibility) { // no vis info, so make all visible
		while (row) {
			*out_p++ = 0xff;
			row--;
		}
		return;
	}

	do {
		if (*in) {
			*out_p++ = *in++;
			continue;
		}

		c = in[1];
		in += 2;
		if ((out_p - out) + c > row) {
			c = row - (out_p - out);
			Com_Warn("Overrun\n");
		}
		while (c) {
			*out_p++ = 0;
			c--;
		}
	} while (out_p - out < row);
}

/*
 * @brief
 */
byte *Cm_ClusterPVS(const int32_t cluster) {
	static byte pvs_row[MAX_BSP_LEAFS / 8];

	if (cluster == -1)
		memset(pvs_row, 0, (cm_vis->num_clusters + 7) >> 3);
	else
		Cm_DecompressVis(cm_bsp.visibility + cm_vis->bit_offsets[cluster][DVIS_PVS], pvs_row);

	return pvs_row;
}

/*
 * @brief
 */
byte *Cm_ClusterPHS(const int32_t cluster) {
	static byte phs_row[MAX_BSP_LEAFS / 8];

	if (cluster == -1)
		memset(phs_row, 0, (cm_vis->num_clusters + 7) >> 3);
	else
		Cm_DecompressVis(cm_bsp.visibility + cm_vis->bit_offsets[cluster][DVIS_PHS], phs_row);

	return phs_row;
}

/*
 *
 * AREA_PORTALS
 *
 */

/*
 * @brief Recurse over the area portals, marking adjacent ones as flooded.
 */
static void Cm_FloodArea(cm_bsp_area_t *area, int32_t flood_num) {
	const d_bsp_area_portal_t *p;
	int32_t i;

	if (area->flood_valid == cm_bsp.flood_valid) {
		if (area->flood_num == flood_num)
			return;
		Com_Error(ERR_DROP, "Reflooded\n");
	}

	area->flood_num = flood_num;
	area->flood_valid = cm_bsp.flood_valid;

	p = &cm_bsp.area_portals[area->first_area_portal];

	for (i = 0; i < area->num_area_portals; i++, p++) {
		if (cm_bsp.portal_open[p->portal_num]) {
			Cm_FloodArea(&cm_bsp.areas[p->other_area], flood_num);
		}
	}
}

/*
 * @brief
 */
void Cm_FloodAreas(void) {
	int32_t i;
	int32_t flood_num;

	// all current floods are now invalid
	cm_bsp.flood_valid++;
	flood_num = 0;

	// area 0 is not used
	for (i = 1; i < cm_bsp.num_areas; i++) {
		cm_bsp_area_t *area = &cm_bsp.areas[i];

		if (area->flood_valid == cm_bsp.flood_valid)
			continue; // already flooded into

		Cm_FloodArea(area, ++flood_num);
	}
}

/*
 * @brief Sets the state of the specified area portal and re-floods all area
 * connections, updating their flood counts such that Cm_WriteAreaBits
 * will return the correct information.
 */
void Cm_SetAreaPortalState(const int32_t portal_num, const _Bool open) {
	if (portal_num > cm_bsp.num_area_portals) {
		Com_Error(ERR_DROP, "Portal %d > num_area_portals", portal_num);
	}

	cm_bsp.portal_open[portal_num] = open;
	Cm_FloodAreas();
}

/*
 * @brief Returns true if the specified areas are connected.
 */
_Bool Cm_AreasConnected(int32_t area1, int32_t area2) {

	if (cm_no_areas)
		return true;

	if (area1 > cm_bsp.num_areas || area2 > cm_bsp.num_areas) {
		Com_Error(ERR_DROP, "Area %d > cm.num_areas\n", area1 > area2 ? area1 : area2);
	}

	if (cm_bsp.areas[area1].flood_num == cm_bsp.areas[area2].flood_num)
		return true;

	return false;
}

/*
 * @brief Writes a bit vector of all the areas that are in the same flood as the
 * specified area. Returns the length of the bit vector in bytes.
 *
 * This is used by the client view to cull visibility.
 */
int32_t Cm_WriteAreaBits(byte *buffer, const int32_t area) {
	int32_t i;
	const int32_t bytes = (cm_bsp.num_areas + 7) >> 3;

	if (cm_no_areas) { // for debugging, send everything
		memset(buffer, 0xff, bytes);
	} else {
		const int32_t flood_num = cm_bsp.areas[area].flood_num;
		memset(buffer, 0, bytes);

		for (i = 0; i < cm_bsp.num_areas; i++) {
			if (cm_bsp.areas[i].flood_num == flood_num || !area) {
				buffer[i >> 3] |= 1 << (i & 7);
			}
		}
	}

	return bytes;
}

/*
 * @brief Returns true if any leaf under head_node has a cluster that
 * is potentially visible.
 */
_Bool Cm_HeadnodeVisible(const int32_t node_num, const byte *vis) {
	const cm_bsp_node_t *node;

	if (node_num < 0) { // at a leaf, check it
		const int32_t leaf_num = -1 - node_num;
		const int32_t cluster = cm_bsp.leafs[leaf_num].cluster;

		if (cluster == -1)
			return false;

		if (vis[cluster >> 3] & (1 << (cluster & 7)))
			return true;

		return false;
	}

	node = &cm_bsp.nodes[node_num];

	if (Cm_HeadnodeVisible(node->children[0], vis))
		return true;

	return Cm_HeadnodeVisible(node->children[1], vis);
}