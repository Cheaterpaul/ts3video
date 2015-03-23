#ifndef TS3VIDEO_H
#define TS3VIDEO_H

/*!
  Global definitions for the project.

  Shortcuts:
    "IFVS_*" stands for "insaneFactory Video Server ..."
 */
#define IFVS_SOFTWARE_VERSION "1.0"               ///< Version of the software
#define IFVS_SOFTWARE_VERSION_POSTFIX "alpha"     ///< Version label (alpha, beta, release, ...)
#define IFVS_SOFTWARE_VERSION_BUILD "0"           ///< Incrementing number with each build, which can be mapped to a revision in source repository.
#define IFVS_SOFTWARE_VERSION_QSTRING QString("%1 %2 (Build %3)").arg(IFVS_SOFTWARE_VERSION).arg(IFVS_SOFTWARE_VERSION_POSTFIX).arg(IFVS_SOFTWARE_VERSION_BUILD)

#endif