#
# mysql-xml-to-csv - Convert MySQL XML output to CSV
#
# Copyright (C) 2021 Archie L. Cobbs. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

bin_PROGRAMS=		mysql-xml-to-csv

man_MANS=		mysql-xml-to-csv.1

docdir=			$(datadir)/doc/packages/$(PACKAGE)

doc_DATA=		CHANGES LICENSE README.md INSTALL AUTHORS

EXTRA_DIST=		CHANGES LICENSE README.md

mysql_xml_to_csv_SOURCES=	main.c \
				gitrev.c

gitrev.c:
			printf 'const char *const mysql_xml_to_csv_version = "%s";\n' "`git describe`" > gitrev.c
