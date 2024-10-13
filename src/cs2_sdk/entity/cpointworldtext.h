/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2024 Source2ZE
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../schema.h"

#include "cbasemodelentity.h"

enum PointWorldTextJustifyHorizontal_t : uint32_t
{
	POINT_WORLD_TEXT_JUSTIFY_HORIZONTAL_LEFT = 0x0,
	POINT_WORLD_TEXT_JUSTIFY_HORIZONTAL_CENTER = 0x1,
	POINT_WORLD_TEXT_JUSTIFY_HORIZONTAL_RIGHT = 0x2,
};

enum PointWorldTextJustifyVertical_t : uint32_t
{
	POINT_WORLD_TEXT_JUSTIFY_VERTICAL_BOTTOM = 0x0,
	POINT_WORLD_TEXT_JUSTIFY_VERTICAL_CENTER = 0x1,
	POINT_WORLD_TEXT_JUSTIFY_VERTICAL_TOP = 0x2,
};

enum PointWorldTextReorientMode_t : uint32_t
{
	POINT_WORLD_TEXT_REORIENT_NONE = 0x0,
	POINT_WORLD_TEXT_REORIENT_AROUND_UP = 0x1,
};

class CPointWorldText : public CBaseModelEntity
{
	DECLARE_SCHEMA_CLASS(CPointWorldText)

	SCHEMA_FIELD(char, m_messageText)
	SCHEMA_FIELD(char, m_FontName)
	SCHEMA_FIELD(bool, m_bEnabled)
	SCHEMA_FIELD(bool, m_bFullbright)
	SCHEMA_FIELD(float, m_flWorldUnitsPerPx)
	SCHEMA_FIELD(float, m_flFontSize)
	SCHEMA_FIELD(float, m_flDepthOffset)
	SCHEMA_FIELD(Color, m_Color)
	SCHEMA_FIELD(PointWorldTextJustifyHorizontal_t, m_nJustifyHorizontal)
	SCHEMA_FIELD(PointWorldTextJustifyVertical_t, m_nJustifyVertical)
	SCHEMA_FIELD(PointWorldTextReorientMode_t, m_nReorientMode)
};