-- -*-sql-*- 
--
-- Copyright (C) 2006 Loic Dachary <loic@dachary.org>
--
-- This software's license gives you freedom; you can copy, convey,
-- propagate, redistribute and/or modify this program under the terms of
-- the GNU Affero General Public License (AGPL) as published by the Free
-- Software Foundation, either version 3 of the License, or (at your
-- option) any later version of the AGPL.
--
-- This program is distributed in the hope that it will be useful, but
-- WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero
-- General Public License for more details.
--
-- You should have received a copy of the GNU Affero General Public License
-- along with this program in a file in the toplevel directory called
-- "AGPLv3".  If not, see <http://www.gnu.org/licenses/>.
--
DROP TABLE IF EXISTS users;

CREATE TABLE users (
  serial INT UNSIGNED NOT NULL AUTO_INCREMENT,
  created TIMESTAMP DEFAULT CURRENT_TIMESTAMP ,
  modified TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,

  PRIMARY KEY (serial),
);
