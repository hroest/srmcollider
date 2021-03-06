#!/usr/bin/python
# -*- coding: utf-8  -*-

"""
 *
 * Program       : SRMCollider
 * Author        : Hannes Roest <roest@imsb.biol.ethz.ch>
 * Date          : 05.02.2011 
 *
 *
 * Copyright (C) 2011 - 2012 Hannes Roest
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307, USA
 *
"""

"""
Dependencies in Python (the following packages must be available):
    MySQLdb
    sqlite

instructions: python setup.py build

sudo apt-get install python-nosexcover 
sudo apt-get install python-dev python-sqlite python-mysqldb
sudo apt-get install libboost-dev
sudo apt-get install libcgal-dev
sudo apt-get install libboost-python-dev 
# to also get the pure C files to compile
sudo apt-get install libboost-filesystem-test libboost-test-dev libboost-system-dev
# to also draw nice images in the web
sudo apt-get install python-matplotlib

#or all at the same time
sudo apt-get install -y python-dev libcgal-dev libboost-python-dev

On Ubuntu, it might fail with 
 /usr/bin/ld: cannot find -lboost_python
then try to do this
sudo ln -s /usr/lib/libboost_python-mt.so /usr/lib/libboost_python.so

"""

# Here we use dynamically linked libraries at runtime, but the problem is one
# has to know where they are at compile time. The easiest thing to do is to put
# them in a place where they are expected, such as /usr/lib/ or /usr/local/lib
# (see /etc/ld.so.conf for a list). 

# Alternatively, one can define this BEFORE starting the python compiler by
# adding the path where the libraries *.so file is to the library search path: 
# LD_LIBRARY_PATH=../:./:$LD_LIBRARY_PATH
# export LD_LIBRARY_PATH
# This will not work from inside python, it has to be done BEFORE starting python.

#these 2.5M combinations should be written to "test.out" in a couple of seconds
#compared to the itertools.combinations there is mainly a speed advantage if the
#strings to be written are short
 
from setuptools import setup
from distutils.extension import Extension

# if your CGAL libraries are somewhere else, please tell us here
# CGAL is currently used header-only, so no need to link
CGAL_libraries = '.'
 
boost_libdir = '.'
boost_includedir = '.'

import fnmatch, os
all_scripts = []
for root, dirnames, filenames in os.walk('scripts/runscripts'):
  for filename in fnmatch.filter(filenames, '*.py'):
      all_scripts.append(os.path.join(root, filename))
all_scripts.extend(['scripts/misc/trypsinize.py'])
all_scripts.extend(['scripts/misc/create_db.py'])

setup(name="srmcollider",
    url = "http://www.srmcollider.org", 
    version = "1.5",
    author = "Hannes Roest",
    author_email = "roest@imsb.biol.ethz.ch",
    requires=["MySQLdb", "sqlite"],

    packages = ['srmcollider', 'srmcollider-webapp'],
    package_dir = {
        'srmcollider-webapp': 'cgi-scripts',
    },

    scripts=all_scripts,
    ext_modules=[
        Extension("srmcollider/c_combinations", ["cpp/py/py_combinations.cpp"],
            include_dirs=["./cpp/", "./cpp/src", boost_includedir],
            library_dirs=["/usr/local/lib/python2.6/dist-packages/", boost_libdir],
            libraries = ["boost_python"]
            ),
        Extension("srmcollider/c_getnonuis", ["cpp/py/py_getNonUis.cpp"], 
            include_dirs=["./cpp/", "./cpp/src", boost_includedir],
            library_dirs=["/usr/local/lib/python2.6/dist-packages/", boost_libdir],
            runtime_library_dirs=["./", "../"],
            libraries = ["boost_python"]),
        Extension("srmcollider/c_rangetree", ["cpp/py/py_rangetree.cpp"],
            include_dirs=["./cpp/", "./cpp/src", CGAL_libraries + '/include/', boost_includedir],
            library_dirs=[CGAL_libraries +'/lib/', boost_libdir],
            libraries = ["boost_python"],
            ),
        Extension("srmcollider/c_integrated", ["cpp/py/py_integratedrun.cpp"], 
            include_dirs=["./cpp/", "./cpp/src", CGAL_libraries + '/include/', boost_includedir],
            library_dirs=[CGAL_libraries +'/lib/', boost_libdir],
            runtime_library_dirs=["./", "../"],
            libraries = ["boost_python"]
            ),
    ],
)

