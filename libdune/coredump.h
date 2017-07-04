
#pragma once

/**
 * Creates a core dump of the virtualized process.
 * 
 * This may only be called when the process is in Dune mode. Calling
 * this method when the process is not in Dune mode yields undefined behavior.
 *
 * The core dumping itself is handled by the linux kernel. Please refer to
 * core(5) for further information on core dumps in linux.
 */
int dune_core_dump();
