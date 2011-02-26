/******************************************************************************
* Copyright (C) 2011 Marc Villacorta Morera
*
* Authors: Marc Villacorta Morera <marc.villacorta@gmail.com>
*
* This file is part of BlackBird.
*
* BlackBird is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* BlackBird is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with BlackBird. If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

//-----------------------------------------------------------------------------
// Include guard:
//-----------------------------------------------------------------------------

#ifndef _BB_FIFO_
#define _BB_FIFO_

//-----------------------------------------------------------------------------
// Includes:
//-----------------------------------------------------------------------------

#include <stdlib.h>

//-----------------------------------------------------------------------------
// Typedefs:
//-----------------------------------------------------------------------------

typedef struct _NODE {

    void            *cptr;
    struct _NODE    *nxt;

} NODE, *PNODE;

typedef struct _FIFO {

    PNODE    cap;
    PNODE    cua;

} FIFO, *PFIFO;

//-----------------------------------------------------------------------------
// Prototypes:
//-----------------------------------------------------------------------------

int bb_fifo_new(PFIFO fifo);
int bb_fifo_empty(PFIFO fifo);
int bb_fifo_push(PFIFO fifo, void *cptr);
void bb_fifo_pop(PFIFO fifo);

//-----------------------------------------------------------------------------
// End of include guard:
//-----------------------------------------------------------------------------

#endif
