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

#include "bb_daemon.h"

//-----------------------------------------------------------------------------
// daemonize:
//-----------------------------------------------------------------------------

void daemonize(void)

{
    pid_t pid;

    // Already a daemon:
    if(getppid() == 1) return;

    // Forking the parent process:
    if((pid = fork()) < 0) exit(1);
    if(pid > 0) exit(0);

    // Setting up the environment:
    umask(0);
    if(setsid() < 0) exit(1);
    if((chdir("/")) < 0) exit(1);

    // Close the std file descriptors:
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}
