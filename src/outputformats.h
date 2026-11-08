/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     outputformats.h

Optional output formats controlled from the global file:
  NODEFILE_FORMAT  TXT | CSV | BOTH   (node files, default TXT)
  MANIFEST_FILE    <name>             (JSON description of the run, default none)

This software is released under the MIT license.
********************************************************************************/

#ifndef HERSS_OUTPUTFORMATS_H
#define HERSS_OUTPUTFORMATS_H

#include "herss.h"

// Run after Herss::WriteNodeOutput(). Converts the node text files to CSV when requested
// and writes the manifest when MANIFEST_FILE is set. Does nothing with the default settings.
int WriteOutputFormats(Herss *herss);

#endif
