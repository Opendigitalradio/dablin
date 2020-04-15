/*
    DABlin - capital DAB experience
    Copyright (C) 2016-2020 Stefan Pöschel

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef VERSION_H_
#define VERSION_H_

#include <stdio.h>

// usually externally derived from git
#ifndef DABLIN_VERSION
#define DABLIN_VERSION "1.13.0"
#endif


void fprint_dablin_banner(FILE *f);

#endif /* VERSION_H_ */
