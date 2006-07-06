<?php
//
// Copyright (C) 2006 Mekensleep
//
// Mekensleep
// 24 rue vieille du temple
// 75004 Paris
//       licensing@mekensleep.com
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301, USA.
//
// Authors:
//  Loic Dachary <loic@gnu.org>
//

/*
 
function myErrorHandler($errno, $errstr, $errfile, $errline) {
  fwrite(STDERR, "ERROR: $errstr [$errno] at $errfile:$errline\n");
  exit(1);
}
set_error_handler("myErrorHandler");

*/

require_once 'currency_configuration.php';

class currency {

  const E_UNKNOWN		=	0;
  const E_INVALID_NOTE		=	1;
  const E_CHANGE_NOTE_FAILED	=	3;
  const E_PUT_NOTE_FAILED	=	4;
  const E_VALUES_UNIT_MISSING	=	5;
  const E_DATABASE_CORRUPTED	=	6;
  const E_RANDOM		=	7;
  const E_COMMIT		=	8;
  const E_TRANSACTION		=	9;

  const MYSQL_ER_DUP_KEY	=	1022;
  const MYSQL_ER_DUP_ENTRY	=	1062;

  const key_size = 20;
  const key_size_ascii = 40;

  var $fixedname = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";

  function __construct($base = FALSE, $user = FALSE, $password = FALSE) {
    $this->db_persist = $GLOBALS['currency_db_persist'];
    $this->db_host = "localhost";
    $this->db_port = 3306;
    $this->db_base = $base;
    $this->db_user = $user;
    $this->db_password = $password;
    $this->db_prefix = "currency";
    $this->db_connection = FALSE;
    $this->db_selected = FALSE;

    $this->set_get_name('get_randname');

    $this->value2table = array();

    $this->trigger_error = FALSE;

    $this->url = $GLOBALS['currency_url'];

    $this->random_fd = fopen($GLOBALS['currency_random'], "r");
    if(!$this->random_fd) throw new Exception("failed to open $currency_random");

    $this->set_values($GLOBALS['currency_values']);
  }

  function __destruct() {
    if($this->db_connection != FALSE) mysql_close($this->connection);
    fclose($this->random_fd);
  }

  function transaction_wrap() {
    try {
      $this->db_query("START TRANSACTION");
      $args = func_get_args();
      $func = array_shift($args); 
      $result = call_user_func_array(array($this, $func), $args);
      $this->db_query("COMMIT");
      return $result;
    } catch(Exception $error) {
      $this->db_query("ROLLBACK");
      throw $error;
    }
  }

  function get_randname() {
    $number = fread($this->random_fd, self::key_size);
    if($number == FALSE)
      throw new Exception("unable to read " . self::key_size . " from " . $GLOBALS['currency_random']);
    return bin2hex($number);
  }

  function get_fixedname() {
    return $this->fixedname;
  }

  function set_url($url) { $this->url = $url; }

  function sort_values($values) {
    $values = array_values($values);
    if(!rsort($values, SORT_NUMERIC)) throw new Exception("sort error on " . $values);
    return $values;
  }

  function set_values($values) { $this->values = $this->sort_values($values); }

  function set_get_name($get_name) { $this->get_name = array(&$this, $get_name); }

  function set_db_persist($persist) { $this->db_check_connection(); $this->db_persist = $persist; }

  function set_db_prefix($prefix) { $this->db_check_connection(); $this->db_prefix = $prefix; }

  function set_db_host($host) { $this->db_check_connection(); $this->db_host = $host; }

  function set_db_port($port) { $this->db_check_connection(); $this->db_port = $port; }

  function set_db_base($base) { $this->db_check_connection(); $this->db_base = $base; }

  function set_db_user($user) { $this->db_check_connection(); $this->db_user = $user; }

  function set_db_password($password) { $this->db_check_connection(); $this->db_password = $password; }

  function db_check_connection() {
    if($this->db_connection == FALSE) {
      $this->connection = call_user_func(($this->db_persist ? 'mysql_pconnect' : 'mysql_connect'),
                                         $this->db_host . ":" . $this->db_port, $this->db_user, $this->db_password);
      if(!$this->connection) throw new Exception("mysql_pconnect " . $this->db_host . ":" . $this->db_port . " user = " . $this->db_user . " password = " . $this->db_password );
      $this->db_connection = TRUE;
    }
  }

  function db_check_selected() {
    if($this->db_selected == FALSE) {
      $this->db_check_connection();
      if(!mysql_query("CREATE DATABASE IF NOT EXISTS " . $this->db_base, $this->connection))
        throw new Exception("db_check_selected(1):" . mysql_error($this->connection));
      if(!mysql_select_db($this->db_base, $this->connection))
        throw new Exception("db_check_selected(2):" . mysql_error($this->connection));

      $sql = "SHOW TABLES LIKE '" . $this->db_prefix . "_%'";
      $result = mysql_query($sql);
      if(!$result) throw new Exception("db_check_selected(3):" . $sql . mysql_error($this->connection));
      $count = mysql_num_rows($result);
      $this->value2table = array();
      $prefix = $this->db_prefix . "_";
      $prefix_length = strlen($prefix);
      if($count != FALSE && $count > 0) {
        while ($row = mysql_fetch_row($result)) {
          $table = $row[0];
          if(strlen($table) <= $prefix_length) // impossible because of the LIKE clause above
            throw Exception("mysql table $table is shorter or equal than the expected prefix $prefix",  self::E_DATABASE_CORRUPTED);
          $value = intval(substr($table, $prefix_length));
          $this->value2table[$value] = $table;
        }
      }
      mysql_free_result($result);

      $unexpected_tables = array_diff(array_keys($this->value2table), $this->values);
      if($unexpected_tables) 
        throw new Exception("mysql tables found for the values " . join(",", $unexpected_tables) . " because the only valid values are " . join(",", $this->values), self::E_DATABASE_CORRUPTED);

      $sql = "CREATE TABLE IF NOT EXISTS " . $this->db_prefix . " ( " .
        "   rowid INT NOT NULL AUTO_INCREMENT, " .
        "   updated TIMESTAMP, " . 
        "   value INT NOT NULL, " . 
        "   randname CHAR(40) NOT NULL UNIQUE, " .
        "   valid CHAR DEFAULT 'n' NOT NULL, " .
        "   PRIMARY KEY (rowid, value, randname) " .
        " ) ENGINE=InnoDB CHARSET=ascii;";
      if(!mysql_query($sql))
        throw new Exception("db_check_selected(4):" . mysql_error($this->connection));

      $sql = "CREATE TABLE IF NOT EXISTS " . $this->db_prefix . "_transactions ( " .
        "   transaction_id CHAR(40) NOT NULL, " .
        "   updated TIMESTAMP, " . 
        "   table_name VARCHAR(255) NOT NULL, " . 
        "   rowid INT NOT NULL, " .
        "   randname CHAR(40) NOT NULL UNIQUE, " .
        "   valid CHAR NOT NULL, " .
        "   INDEX ( transaction_id ) " .
        " ) ENGINE=InnoDB CHARSET=ascii;";
      if(!mysql_query($sql))
        throw new Exception("db_check_selected(4):" . mysql_error($this->connection));

      $this->db_selected = TRUE;
    }
  }

  function value2table($value) {
    if(in_array($value, $this->values, TRUE)) {
      return $this->db_prefix . "_${value}";
    } else {
      return $this->db_prefix;
    }
  }

  function is_private_table($table) {
    return $table != $this->db_prefix;
  }
  
  function db_check_or_create_table_value($value) {
    $this->db_check_selected();
    $value = intval($value);
    if(array_key_exists($value, $this->value2table)) {
      return;
    } elseif(in_array($value, $this->values, TRUE)) { 
      $table = $this->value2table($value);
      $sql = "CREATE TABLE IF NOT EXISTS ${table} ( " .
        "   rowid INT NOT NULL AUTO_INCREMENT, " .
        "   updated TIMESTAMP, " . 
        "   randname CHAR(40) NOT NULL UNIQUE, " .
        "   valid CHAR DEFAULT 'n' NOT NULL, " .
        "   PRIMARY KEY (rowid, randname)  " . 
        " ) ENGINE=InnoDB CHARSET=ascii;";
      if(!mysql_query($sql))
        throw new Exception("db_check_or_create_table_value(1):" . mysql_error($this->connection));
      $this->value2table[$value] = TRUE;
    } elseif($value <= 0) {
      throw new Exception("value = $value must be a positive integer");
    }
  }

  function db_check_table_value($value) {
    $this->db_check_selected();
    if(in_array($value, $this->values, TRUE)) {
      $table = $this->value2table($value);
      $result = mysql_query("SHOW TABLES LIKE '$table'");
      if(!$result) throw new Exception("db_check_table_value: SHOW TABLES LIKE '$table' " . mysql_error($this->connection));
      $count = mysql_num_rows($result);
      if($count == FALSE || $count == 0) throw new Exception("$table does not exist ", self::E_INVALID_NOTE);
      if($count > 1) throw new Exception("table $table duplicate ?", self::E_INVALID_NOTE);
      mysql_free_result($result);
    }
  }

  function db_query($query) {
    $this->db_check_selected();
    $result = mysql_query($query, $this->connection);
    if(!$result) throw new Exception("db_query: " . $query . ": " . mysql_error($this->connection));
    return $result;
  }

  function get_note($value) {
    return $this->transaction_wrap('_get_note_transaction', $value);
  }

  function _get_note_transaction($value) {
    $note = $this->_get_note($value);
    $this->_transaction_add($note[2], 'n', $note);
    return $note;
  }

  function _get_note($value) {
    $value = intval($value);
    $done = FALSE;
    $this->db_check_or_create_table_value($value);
    $table = $this->value2table($value);
    $is_private_table = $this->is_private_table($table);
    $retry = 0;
    $randname = FALSE;
    while(!$done && $retry < 5) {
      $randname = call_user_func($this->get_name, $this);
      if($is_private_table)
        $sql = "INSERT INTO ${table} (randname) VALUES ('$randname')";
      else
        $sql = "INSERT INTO ${table} (value, randname) VALUES ($value, '$randname')";
      $status = mysql_query($sql, $this->connection);
      if($status) {
        $done = TRUE;
      } elseif(mysql_errno($this->connection) == self::MYSQL_ER_DUP_KEY ||
               mysql_errno($this->connection) == self::MYSQL_ER_DUP_ENTRY) {
        //
        // Although highly unlikely, it is possible to have a name clash
        // Allow for 5 tries before failure
        //
        $retry++;
      } else {
        throw new Exception("get_note: " . $sql . " : " . mysql_error($this->connection) . " : " . mysql_errno());
      }
    }
    if($retry >= 5)
      throw new Exception("Unable to insert a note in the database (last name was $randname). This probably indicates a problem with the random feed " . $GLOBALS['currency_random'], self::E_RANDOM);
    $insert_id = mysql_insert_id($this->connection);
    return array($this->url, $insert_id, $randname, $value);
  }

  function commit($transaction_id) {
    if(strlen($transaction_id) != self::key_size_ascii || preg_match ("|^\w{40}$|", $transaction_id) == 0)
      throw new Exception("$transaction_id is not a valid transaction name");
    $sql = "SELECT table_name, rowid, randname, valid FROM " . $this->db_prefix . "_transactions " .
      "            WHERE transaction_id = '" . $transaction_id . "'";
    $status = mysql_query($sql);
    if($status) {
      $transactions_count = mysql_affected_rows($this->connection);
      if($transactions_count == 0)
        throw new Exception("no rows for transaction $transaction_id", self::E_COMMIT);
      //
      // Prepare all the SQL statements to commit the transaction
      // (i.e. change the toggle 'y' and 'n' value of each valid
      // field.
      //
      $sqls = array();
      while(list($table_name, $rowid, $randname, $valid) = mysql_fetch_array($status, MYSQL_NUM)) {
        $key = "${table_name}-${valid}";
        if(!array_key_exists($key, $sqls)) {
          $future_valid = $valid == 'y' ? 'n' : 'y';
          $sqls[$key] = array("UPDATE $table_name SET valid = '$future_valid' WHERE valid = '$valid' AND rowid in ( ");
        }
        array_push($sqls[$key], strval($rowid));
      }
      return $this->transaction_wrap('_commit', $transaction_id, $sqls, $transactions_count);
    } else {
      throw new Exception("failed to commit transaction $transaction_id" . mysql_error($this->connection), self::E_COMMIT);
    }
  }

  function _commit($transaction_id, $sqls, $transactions_count) {
    //
    // Run the SQL statements, verifying that each of them affect
    // the expected number of rows.
    //
    foreach ( $sqls as $sql ) {
      $expected_affected_rows = count($sql) - 1;
      $sql_string = array_shift($sql);
      $sql_string .= join(", ", $sql);
      $sql_string .= " ) ";
      $status = mysql_query($sql_string);
      if(mysql_affected_rows($this->connection) != $expected_affected_rows) 
        throw new Exception("$sql_string affected " . mysql_affected_rows($this->connection) . " instead of the expected $expected_affected_rows", self::E_COMMIT);
    }
    //
    // Remove the transaction records
    //
    $sql = "DELETE FROM " . $this->db_prefix . "_transactions WHERE transaction_id = '$transaction_id'";
    mysql_query($sql);
    if(mysql_affected_rows($this->connection) != $transactions_count) 
      throw new Exception("$sql  affected " . mysql_affected_rows($this->connection) . " instead of the expected $transactions_count", self::E_COMMIT);
    return TRUE;
  }

  function _transaction_add($transaction_id, $valid, $note) {
    $table = $this->value2table($note[3]);
    list( $url, $serial, $name, $value ) = $note;
    $sql = "INSERT INTO " . $this->db_prefix . "_transactions " . 
      " ( transaction_id,    table_name,    rowid,   randname, valid ) VALUES " .
      " ( '$transaction_id', '$table',      $serial, '$name',  '$valid' ) ";
    $status = mysql_query($sql);
    if(mysql_affected_rows($this->connection) != 1)
      throw new Exception("failed to $sql " . mysql_error($this->connection), self::E_TRANSACTION);
  }

  function check_note($serial, $name, $value) {
    return $this->_check_note(array($this->url, $serial, $name, $value));
  }

  function _check_note($note) {
    list( $url, $serial, $name, $value ) = $note;

    $serial = intval($serial);
    $value = intval($value);
    $this->db_check_table_value($value);
    $table = $this->value2table($value);
    $status = mysql_query("SELECT COUNT(*) FROM ${table} WHERE rowid = $serial AND randname = '$name' AND valid = 'y'");
    if($status) {
      if(mysql_affected_rows($this->connection) != 1)
        throw new Exception("failed to check note $serial $name" . mysql_error($this->connection), self::E_CHECK_NOTE_FAILED);
    } else {
      throw new Exception("failed to check note $serial $name" . mysql_error($this->connection), self::E_CHECK_NOTE_FAILED);
    }
    return TRUE;
  }

  function change_note($serial, $name, $value) {
    $this->check_note($serial, $name, $value);
    $note = $this->_get_note_transaction($value);
    $this->_transaction_add($note[2], 'y', array($this->url, $serial, $name, $value));
    return $note;
  }

  function put_note($serial, $name, $value) {
    $serial = intval($serial);
    $value = intval($value);
    $this->db_check_table_value($value);
    $table = $this->value2table($value);
    $this->db_query("UPDATE ${table} SET valid = 'n' WHERE rowid = $serial AND randname = '$name' AND valid = 'y'");
    if(mysql_affected_rows($this->connection) != 1)
      throw new Exception("failed to delete note $serial $name $value (affected " . mysql_affected_rows($this->connection) . " rows) " . mysql_error($this->connection), self::E_PUT_NOTE_FAILED);
  }

  function break_note($serial, $name, $value, $values = FALSE) {
    return $this->_transaction_wrap('_break_note', $serial, $name, $value, $values);
  }

  function _break_note($serial, $name, $value, $values) {
    $serial = intval($serial);
    $value = intval($value);
    if($values == FALSE) 
      $values = $this->values;
    else
      $values = $this->sort_values($values);
    $to_delete = array($this->url, $serial, $name, $value);
    $notes = array();
    foreach ( $values as $note_value ) {
      if($value < $note_value) continue;
      $count = intval($value / $note_value); 
      $value %= $note_value;
      for($i = 0; $i < $count; $i++)
        array_push($notes, $this->_get_note($note_value));
      if($value <= 0) break;
    }

    if($value > 0)
      array_push($notes, $this->get_note($value));

    $transaction_id = $notes[0][2];
    $this->_check_note($to_delete);
    $this->_transaction_add($transaction_id, 'y', $to_delete);
    foreach ( $notes as $note )
      $this->_transaction_add($transaction_id, 'n', $note);

    if($this->trigger_error) throw new Exception("unit test exception");
    return $notes;
  }

  function merge_notes_columns($serials, $names, $values, $known_values = FALSE) {
    if(count($serials) != count($names) || count($names) != count($values))
      throw new Exception("serials, names and values must have the same size count(serials) = " . count($serials) . " count(names) = " . count($names) . " count(values) = " . count($values));
    $notes = array();
    for($i = 0; $i < count($serials); $i++)
      array_push($notes, array($this->url, intval($serials[$i]), $names[$i], intval($values[$i])));
    return $this->merge_notes($notes, $known_values);
  }

  function merge_notes($notes, $values = FALSE) {
    if($values == FALSE) 
      $values = $this->values;
    else
      $values = $this->sort_values($values);
    $total = array_reduce($notes, create_function('$a,$b', 'return $a + $b[3];'), 0);
    $value2count = array();
    foreach ( $values as $note_value ) {
      if($total < $note_value) continue;
      $count = intval($total / $note_value); 
      $total %= $note_value;
      $value2count[$note_value] = $count;
      if($total <= 0) break;
    }
    if($total > 0)
      $value2count[$total] = 1;

    // don't merge if this results in a larger number of notes
    if(count($value2count) >= count($notes))
      throw new Exception("merge notes would not merge anything");

    return $this->transaction_wrap('_merge_notes', $value2count, $notes, $values);
  }

  function _merge_notes($value2count, $notes, $value) {
    $new_notes = array();
    $to_delete = array();
    if($values) {
      //
      // Delete the notes provided in argument or re-use them when possible
      //
      foreach ( $notes as $note ) {
        $value = $note[3];
        if(array_key_exists($value, $value2count)) {
          $value2count[$value]--;
          array_push($new_notes, $note);
          if($value2count[$value] <= 0) 
            unset($value2count[$value]);
        } else {
          $this->_check_note($note);
          array_push($to_delete, $note);
        }
      }
    }

    //
    // Create new notes
    //
    foreach ( $value2count as $value => $count ) {
      for($i = 0; $i < $count; $i++)
        array_push($new_notes, $this->_get_note($value));
    }

    $transaction_id = $new_notes[0][2];
    foreach ( $to_delete as $note )
      $this->_transaction_add($transaction_id, 'y', $note);
    foreach ( $new_note as $note )
      $this->_transaction_add($transaction_id, 'n', $note);

    if($this->trigger_error) throw new Exception("unit test exception");
    return $new_notes;
  }
}

?>
