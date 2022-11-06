// This file is part of InK.
// 
// author = "Kasım Sinan Yıldırım " 
// maintainer = "Kasım Sinan Yıldırım "
// email = "sinanyil81 [at] gmail.com" 
//  
// copyright = "Copyright 2018 Delft University of Technology" 
// license = "LGPL" 
// version = "3.0" 
// status = "Production"
//
// 
// InK is free software: you ca	n redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "ink.h"

// indicates if this is the first boot.
__nv uint8_t __not_first_boot;

// functions to initialize non-volatile global variables
#if KERNEL_FRAM
// extern void __initISRManagerGlobalVars();
extern void __initSchedulerGlobalVars();
#endif

// this is the entry function for the application initialization.
// applications should implement it.
extern void __app_init();
extern void __app_reboot();

int __hwReboot(void) {
    uint8_t isNotFirstBoot = FALSE;
    __NVM_GET(isNotFirstBoot, __not_first_boot);
    
    if(!isNotFirstBoot) {
        KERNEL_LOG_INFO("First Boot");
        
		__scheduler_boot_init();
	    
		// init the event handler
	    __events_boot_init();

	    // init the applications
	    __app_init();

        /* initilize all global NVM variables here */
        __NVM_SET(__not_first_boot, TRUE);

#if KERNEL_FRAM
        // __initISRManagerGlobalVars();
        __initSchedulerGlobalVars();
#endif

    }
    else {
        KERNEL_LOG_INFO("Not First Boot");
    }

    return 0;
}

int main(void)
{
	// will be called at each reboot of the application
	__app_reboot();

	// activate the scheduler
	__scheduler_run();

	return 0;
}
