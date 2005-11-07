/*
 * Copyright (C) 2002-5 by the Widelands Development Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef __S__VARIABLE_MANAGER_H
#define __S__VARIABLE_MANAGER_H

#include "error.h"
#include <string>

#include "ui/ui_basic/ui_object.h" //just for i18n

/*
 * The Map Variable Manager makes sure that variables
 * in the map are watched accordingly.
 *
 * A Map variable is a user defined type, which can be changed with 
 * triggers or events and displayed in strings. Also triggers can be 
 * run when variables are over a certain value. 
 *
 * Variables are mostly for mission objectives - simple stuff (like how many
 * buildings of a type should be build before triggering) doesn't use variables
 */

/*
 * First, the variables
 */
class MapVariable {
   public:
      enum Type {
         MVT_INT,
         MVT_STRING,
      };

   public:
      MapVariable( bool t ) { m_delete_protected = t; } 
      virtual ~MapVariable( void ) { }

      inline bool is_delete_protected( void ) { return m_delete_protected; }
      inline const char* get_name( void ) { return m_name.c_str(); }
      inline void set_name( const char* name ) { m_name = name; }

      virtual Type get_type( void ) = 0;
      
   private:
      std::string   m_name;
      bool           m_delete_protected;
};

class Int_MapVariable : public MapVariable {
   public:
      Int_MapVariable( bool t ) : MapVariable( t ) { m_value = 0; }

      virtual Type get_type( void ) { return MVT_INT; }

      long get_value( void ) { return m_value; }
      void set_value( long t ) { m_value = t; }

   private:
      long            m_value;
};

class String_MapVariable : public MapVariable {
   public:
      String_MapVariable( bool t ) : MapVariable( t ) { m_value = ""; }

      virtual Type get_type( void ) { return MVT_STRING; }

      const char* get_value( void ) { return m_value.c_str(); }
      void set_value( const char* t ) { m_value = t; }

   private:
      std::string            m_value;
};

/*
 * The manager himself. 
 * This is mainly a wrapper, the function
 * could have also been implemented directly in the map.
 *
 * But it is better this way.
 */
class MapVariableManager {
   public:
      MapVariableManager( void );
      ~MapVariableManager( void );

      /*
       * Register a new variable
       */
      bool register_new_variable( MapVariable* );

      /*
       * This prevents casting
       */
      Int_MapVariable* get_int_variable( const char* name );
      String_MapVariable* get_string_variable( const char* name );
     
      /*
       * Get a variable
       */
      MapVariable* get_variable( const char* name );
      void delete_variable( const char* name );

      inline int get_nr_variables( void ) { return m_variables.size(); }
      inline MapVariable* get_variable_by_nr( int i ) { assert(i < (int)m_variables.size()); return m_variables[i]; }

   private:
      std::vector<MapVariable*>      m_variables; 
};

#endif

