/***************************************************************************
 *   Copyright (C) 2023 by Radu Andries                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/

#pragma once

#ifdef CRYPTOPP_NAMING
    #include <cryptopp/sha.h>
    #include <cryptopp/hex.h>
    #include <cryptopp/files.h>
    #include <cryptopp/filters.h>
#else
    #include <crypto++/sha.h>
    #include <crypto++/hex.h>
    #include <crypto++/files.h>
    #include <crypto++/filters.h>
#endif // CRYPTOPP_NAMING
