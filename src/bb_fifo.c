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
// Includes:
//-----------------------------------------------------------------------------

#include "bb_fifo.h"

//-----------------------------------------------------------------------------
// bb_fifo_new:
//-----------------------------------------------------------------------------

int bb_fifo_new(PFIFO fifo)

{
    if((fifo->cap = malloc(sizeof(NODE))) == NULL) return 0;
    fifo->cua = fifo->cap;
    fifo->cua->nxt = NULL;
    return 1;
}

//-----------------------------------------------------------------------------
// bb_fifo_push:
//-----------------------------------------------------------------------------

int bb_fifo_push(PFIFO fifo, PCLIENT cptr)

{
    PNODE ptr;

    if((ptr = malloc(sizeof(NODE))) == NULL) return 0;
    ptr->cptr = cptr;
    ptr->nxt = NULL;
    fifo->cua->nxt = ptr;
    fifo->cua = ptr;
    return 1;
}

//-----------------------------------------------------------------------------
// bb_fifo_pop:
//-----------------------------------------------------------------------------

int bb_fifo_pop(PFIFO fifo)

{
    PNODE ptr;

    if(fifo->cap == fifo->cua) return 0;
    ptr = fifo->cap;
    fifo->cap = ptr->nxt;
    free(ptr);
    return 1;
}
