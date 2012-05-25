
   /*--------------------------------------------------------------------+
    |                              Clay                                  |
    |--------------------------------------------------------------------|
    |                             Clay.c                                 |
    |--------------------------------------------------------------------|
    |                    First version: 03/04/2012                       |
    +--------------------------------------------------------------------+

 +--------------------------------------------------------------------------+
 |  / __)(  )    /__\ ( \/ )                                                |
 | ( (__  )(__  /(__)\ \  /         Chunky Loop Alteration wizardrY         |
 |  \___)(____)(__)(__)(__)                                                 |
 +--------------------------------------------------------------------------+
 | Copyright (C) 2012 University of Paris-Sud                               |
 |                                                                          |
 | This library is free software; you can redistribute it and/or modify it  |
 | under the terms of the GNU Lesser General Public License as published by |
 | the Free Software Foundation; either version 2.1 of the License, or      |
 | (at your option) any later version.                                      |
 |                                                                          |
 | This library is distributed in the hope that it will be useful but       |
 | WITHOUT ANY WARRANTY; without even the implied warranty of               |
 | MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser  |
 | General Public License for more details.                                 |
 |                                                                          |
 | You should have received a copy of the GNU Lesser General Public License |
 | along with this software; if not, write to the Free Software Foundation, |
 | Inc., 51 Franklin Street, Fifth Floor,                                   |
 | Boston, MA  02110-1301  USA                                              |
 |                                                                          |
 | Clay, the Chunky Loop Alteration wizardrY                                |
 | Written by Joel Poudroux, joel.poudroux@u-psud.fr                        |
 +--------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <osl/scop.h>
#include <osl/statement.h>
#include <osl/relation.h>
#include <clay/array.h>
#include <clay/beta.h>



/* 
 * clay_beta_normalize function:
 * Normalize all the beta
 * \param[in] scop
 */
void clay_beta_normalize(osl_scop_p scop) {
  
  osl_statement_p sout;
  osl_relation_p scattering;
  clay_array_p beta;
  clay_array_p beta_last;
  
  int counter_loops[CLAY_MAX_BETA_SIZE];
  int i;
  int row;
  
  for (i = 0 ; i < CLAY_MAX_BETA_SIZE ; i++) {
    counter_loops[i] = 0;
  }
  
  // first statement
  beta_last = clay_array_malloc(); // empty beta
  beta = clay_beta_next(scop->statement, beta_last, &sout);
  
  if (beta == NULL) {
    clay_array_free(beta_last);
    return;
  }
  
  // for each statement, we set the smallest beta
  while (1) {
    // set the smallest beta
    for (i = 0 ; i < beta->size ; i++) {
      beta->data[i] = counter_loops[i];
      row = clay_statement_get_line(sout, i*2);
      scattering = sout->scattering;
      osl_int_set_si(scattering->precision, scattering->m[row],
                     scattering->nb_columns-1, counter_loops[i]);
    }
    
    clay_array_free(beta_last);
    beta_last = beta;
    beta = clay_beta_next(scop->statement, beta_last, &sout);
    
    if (beta == NULL) {
      clay_array_free(beta_last);
      return;
    }
    
    // update the counter array for the next step
    
    // search the first value wich is greater than the beta_last
    i = 0;
    while (i < beta->size && i < beta_last->size) {
      if (beta->data[i] > beta_last->data[i])
        break;
      i++;
    }
    
    if (i != beta->size)
      counter_loops[i]++;
    
    // the rest is set to zero
    i++;
    while (i < beta_last->size) {
      counter_loops[i] = 0;
      i++;
    }
    
  }
}


/**
 * clay_beta_get function:
 * \param[in] statement     Current statement
 * \return                  Return the real line
 */
clay_array_p clay_beta_get(osl_statement_p statement) {
  if (statement == NULL)
    return NULL;

  osl_relation_p scattering = statement->scattering;
  clay_array_p beta = clay_array_malloc();
  int i, j,  row, tmp;
  int last_column = scattering->nb_columns-1;
  int precision = scattering->precision;
  
  for (j = 1 ; j < scattering->nb_output_dims+1 ; j += 2) {
    // search the first non zero
    for (i = 0 ; i < scattering->nb_rows ; i++) {
      // test if the line is an equality
      if (osl_int_zero(precision, scattering->m[i], 0)) {
        // non zero on the column
        if (!osl_int_zero(precision, scattering->m[i], j)) {
          // line and column are corrects
          if (clay_scattering_check_zeros(statement, i, j)) {
            row = clay_statement_get_line(statement, i);
            tmp = osl_int_get_si(scattering->precision, 
                                 scattering->m[row], last_column);
            clay_array_add(beta, tmp);
            break;
          }
        }
      }
    }
  }

  return beta;
}


/**
 * clay_beta_max function:
 * Return beta min
 * \param[in] statement     List of statements
 * \param[in] beta          Beta vector parent
 * \return
 */
clay_array_p clay_beta_min(osl_statement_p statement, clay_array_p beta) {
  statement = clay_beta_find(statement, beta);
  if (!statement)
    return NULL;
  
  clay_array_p beta_min = clay_beta_get(statement);
  
  while (statement != NULL) {
    if (clay_beta_check(statement, beta) &&
        clay_statement_is_before(statement, beta_min)) {
      clay_array_free(beta_min);
      beta_min = clay_beta_get(statement);
    }
    statement = statement->next;
  }
  
  return beta_min;
}


/**
 * clay_beta_max function:
 * Return the beta max
 * \param[in] statement     List of statements
 * \param[in] beta          Beta vector parent
 * \return
 */
clay_array_p clay_beta_max(osl_statement_p statement, clay_array_p beta) {
  statement = clay_beta_find(statement, beta);
  if (!statement)
    return NULL;
  
  clay_array_p beta_max = clay_beta_get(statement);
  
  while (statement != NULL) {
    if (clay_beta_check(statement, beta) &&
        clay_statement_is_after(statement, beta_max)) {
      clay_array_free(beta_max);
      beta_max = clay_beta_get(statement);
    }
    statement = statement->next;
  }
  
  return beta_max;
}


/**
 * clay_beta_next function:
 * Return the beta after the given beta.
 * \param[in] statement     List of statements
 * \param[in] beta          Beta vector
 * \param[in] sout          If not NULL, this is the statement corresponding to 
 *                          the returned beta
 * \return
 */
clay_array_p clay_beta_next(osl_statement_p statement, clay_array_p beta,
                            osl_statement_p *sout) {
  if(!statement)
    return NULL;
  clay_array_p beta_next = NULL;
  const int nb_dims = beta->size*2-1;
  int is_statement;
  
  // Search the first statement after the beta
  while (statement != NULL) {
    is_statement = statement->scattering->nb_output_dims <= nb_dims;
    if ((!is_statement && !clay_statement_is_before(statement, beta)) ||
        (is_statement && clay_statement_is_after(statement, beta))) {
      beta_next = clay_beta_get(statement);
      if (sout) *sout = statement;
      break;
    }
    statement = statement->next;
  }
  
  if (beta_next == NULL) {
    if (sout) *sout = NULL;
    return NULL;
  }
  if (is_statement && 
      statement->scattering->nb_output_dims < beta_next->size*2-1) {
    clay_array_free(beta_next);
    if (sout) *sout = NULL;
    return NULL;
  }

  // Search if there is an another beta before the beta we have found
  while (statement != NULL) {
    is_statement = statement->scattering->nb_output_dims <= nb_dims;
    if (((!is_statement && !clay_statement_is_before(statement, beta)) ||
         (is_statement && clay_statement_is_after(statement, beta))) &&
        clay_statement_is_before(statement, beta_next)) {
      clay_array_free(beta_next);
      beta_next = clay_beta_get(statement);
      if (sout) *sout = statement;
    }
    statement = statement->next;
  }
  
  return beta_next;
}


/**
 * clay_beta_next_part function:
 * Return the beta after (strictly) the given beta.
 * If the beta is a loop, the next beta is the beta which is after the loop (and
 * not the statements inside the loop)
 * \param[in] statement     List of statements
 * \param[in] beta          Beta vector
 * \return
 */
clay_array_p clay_beta_next_part(osl_statement_p statement, clay_array_p beta) {
  if(!statement)
    return NULL;
    
  clay_array_p beta_next = NULL;
  
  // Search the first statement after the beta
  while (statement != NULL) {
    if (clay_statement_is_after(statement, beta)) {
      beta_next = clay_beta_get(statement);
      break;
    }
    statement = statement->next;
  }
  
  if (beta_next == NULL)
    return NULL;
  
  // Search if there is an another beta before the beta we have found
  while (statement != NULL) {
    if (clay_statement_is_after(statement, beta) && 
        clay_statement_is_before(statement, beta_next)) {
      clay_array_free(beta_next);
      beta_next = clay_beta_get(statement);
    }
    statement = statement->next;
  }
  
  return beta_next;
}


/**
 * clay_beta_find function:
 * Return the first statement (the first checked by the beta and not the first 
 * in the list, use clay_beta_min for this purpose) which corresponds to the 
 * beta vector
 * \param[in] statement     Start statement list
 * \param[in] beta          Vector to search
 * \return                  Return the first corresponding statement
 */
osl_statement_p clay_beta_find(osl_statement_p statement, 
                               clay_array_p beta) {
  while (statement != NULL) {
    if (clay_beta_check(statement, beta))
      break;
    statement = statement->next;
  }
  return statement;
}


/**
 * clay_beta_nb_parts function:
 * It doesn't count statements inside sub loops
 * Example:
 * for(i) {
 *   S1(i)
 *   for(j) {
 *     S2(i,j)
 *     S3(i,j)
 *   }
 * }
 * nb_parts in for(i) = 2 (S1 and for(j))
 * \param[in] statement     Statements list
 * \param[in] beta          Vector to search
 * \return
 */
int clay_beta_nb_parts(osl_statement_p statement, clay_array_p beta) {
  int n = 0;
  const int column = beta->size*2;
  int last_level = -1;
  int current_level;
  int row;
  osl_relation_p scattering;
  while (statement != NULL) {
    if (clay_beta_check(statement, beta)) {
      scattering = statement->scattering;
      if (column < scattering->nb_output_dims) {
        row = clay_statement_get_line(statement, column);
        current_level = osl_int_get_si(scattering->precision,
                                       scattering->m[row],
                                       scattering->nb_columns-1);
        if (current_level != last_level) {
          n++;
          last_level = current_level;
        }
      } else if (column-2 >= scattering->nb_output_dims-1) {
        // it's a statement
        return 1;
      }
    }
    statement = statement->next;
  }
  return n;
}


/**
 * clay_beta_shift_before function:
 * Shift all the statements that are before or on the beta vector
 * \param[in] statement     Statements list
 * \param[in] beta          Beta vector 
 * \param[in] depth         Depth level to shift, >= 1
 */
void clay_beta_shift_before(osl_statement_p statement, clay_array_p beta,
                           int depth) {
  if (beta->size == 0)
    return;
  osl_relation_p scattering;
  const int column = (depth-1)*2;  // transition column
  int row;
  int precision = statement->scattering->precision;
  
  clay_array_p beta_parent = clay_array_clone(beta);
  (beta_parent)->size = depth-1;
  
  while (statement != NULL) {
    if (!clay_statement_is_before(statement, beta)) {
      if (clay_beta_check(statement, beta_parent)) {
        scattering = statement->scattering;
        if (column < scattering->nb_output_dims) {
          row = clay_statement_get_line(statement, column);
          osl_int_increment(precision, 
                            scattering->m[row], scattering->nb_columns-1, 
                            scattering->m[row], scattering->nb_columns-1);
        }
      }
    }
    statement = statement->next;
  }
  clay_array_free(beta_parent);
}


/**
 * clay_beta_shift_after function:
 * Shift all the statements that are after or on the beta vector
 * \param[in] statement     Statements list
 * \param[in] beta          Beta vector 
 * \param[in] depth         Depth level to shift, >= 1
 */
void clay_beta_shift_after(osl_statement_p statement, clay_array_p beta,
                           int depth) {
  if (beta->size == 0)
    return;
  osl_relation_p scattering;
  const int column = (depth-1)*2;  // transition column
  int row;
  int precision = statement->scattering->precision;
  
  clay_array_p beta_parent = clay_array_clone(beta);
  beta_parent->size = depth-1;
  
  while (statement != NULL) {
    if (clay_statement_is_after(statement, beta)) {
      scattering = statement->scattering;
      if (column < scattering->nb_output_dims) {
        row = clay_statement_get_line(statement, column);
        osl_int_increment(precision, 
                          scattering->m[row], scattering->nb_columns-1, 
                          scattering->m[row], scattering->nb_columns-1);
      }
    }
    statement = statement->next;
  }
  clay_array_free(beta_parent);
}


/**
 * clay_statement_is_before function:
 * Return true if the statement is before (strictly) the beta
 * \param[in] statement
 * \param[in] beta
 * \return
 */
bool clay_statement_is_before(osl_statement_p statement, clay_array_p beta) {
  osl_relation_p scat = statement->scattering;
  int precision = scat->precision;
  int row;
  int i;
  int end;
  
  if (scat->nb_output_dims >= beta->size*2+1)
    end = beta->size;
  else
    end = (scat->nb_output_dims+1)/2;

  void *tmp;
  tmp = osl_int_malloc(precision);
  
  for (i = 0 ; i < end ; i++) {
    row = clay_statement_get_line(statement, i*2);
    osl_int_add_si(precision,
                   tmp, 0,
                   scat->m[row], scat->nb_columns-1,
                   -beta->data[i]); // -> substraction
    if (osl_int_pos(precision, tmp, 0)) {
      osl_int_free(precision, tmp, 0);
      return 0;
    } else if (osl_int_neg(precision, tmp, 0)) {
      osl_int_free(precision, tmp, 0);
      return 1;
    }
  }
  osl_int_free(precision, tmp, 0);
  return 0;
}


/**
 * clay_statement_is_after function:
 * Return true if statement is after (strictly) the beta
 * \param[in] statement
 * \param[in] beta
 * \return
 */
bool clay_statement_is_after(osl_statement_p statement, clay_array_p beta) {
  return !clay_statement_is_before(statement, beta) && 
         !clay_beta_check(statement, beta);
}


/**
 * clay_beta_check function:
 * check if the current statement corresponds to the beta vector
 * \param[in] statement     Statement to test
 * \param[in] beta          Vector to search
 * \return                  true if correct
 */
bool clay_beta_check(osl_statement_p statement, clay_array_p beta) {
  if (beta->size == 0)
    return 1;
  
  int end;
  if (statement->scattering->nb_output_dims >= beta->size*2+1)
    end = beta->size;
  else
    end = (statement->scattering->nb_output_dims+1)/2;
  
  //if (beta->size*2-1 > statement->scattering->nb_output_dims)
  //  return 0;
  
  osl_relation_p scattering = statement->scattering;
  int last_column = scattering->nb_columns-1;
  int i, j = 1, k;
  int ok;
  int tmp;
  int precision = scattering->precision;
  
  for (k = 0 ; k < end ; k++) {
    ok = 0;
    
    // search the first non zero
    for (i = 0 ; i < scattering->nb_rows ; i++) {
      
      // test if the line is an equality
      if (osl_int_zero(precision, scattering->m[i], 0)) {
        
        // non zero on the column
        if (!osl_int_zero(precision, scattering->m[i], j)) {
          tmp = osl_int_get_si(precision, scattering->m[i], last_column);
          if (tmp != beta->data[k]) {
            return 0;
          }
          
          // line and column are corrects ?
          if (!clay_scattering_check_zeros(statement, i, j)) {
            // errors should not happen
            return 0;
          }
          
          ok = 1;
          break;
        }
      }
    }
    
    if (!ok)
      return 0;
    
    j += 2;
  }
  return 1;
}


/**
 * clay_scattering_check_zeros function:
 * check if the scattering on the first statement contains only zero at line i 
 * and column j, but not on (i,j), on the first column (equality column), and on
 * the last column.
 * Used to check if the line is a line of a beta vector
 * \param[in] statement
 * \param[in] i
 * \param[in] j
 * \return                  true if correct
 */
bool clay_scattering_check_zeros(osl_statement_p statement, int i, int j) {
  osl_relation_p scattering = statement->scattering;
  int precision = scattering->precision;
  int t;
  
  for (t = i+1 ; t < scattering->nb_rows ; t++) {
    if (!osl_int_zero(precision, scattering->m[t], j)) {
      fprintf(stderr, "[Clay] Warning: the scattering is incorrect (column %d)\n",
              j);
      fprintf(stderr, "[Clay] a non-zero value appear\n");
      osl_statement_p tmp = statement->next;
      statement->next = NULL;
      osl_statement_dump(stderr, statement);
      statement->next = tmp;
      return 0;
    }
  }
  for (t = j+1 ; t < scattering->nb_columns-1 ; t++) {
    if (!osl_int_zero(precision, scattering->m[i], t)) {
      fprintf(stderr, "[Clay] Warning: the scattering is incorrect (line %d)\n",
              i+1);
      fprintf(stderr, "[Clay] a non-zero value appear\n");
      osl_statement_p tmp = statement->next;
      statement->next = NULL;
      osl_statement_dump(stderr, statement);
      statement->next = tmp;
      return 0;
    }
  }
  
  return 1;
}


/**
 * clay_statement_get_line function:
 * Because the lines in the scattering matrix may have not ordered, we have to
 * search the corresponding line. It returns the first line where the value is
 * different from zero in the `column'. `column' is between 0 and 
 * nb_output_dims-1
 * \param[in] statement     Current statement
 * \param[in] column        Line to search
 * \return                  Return the real line
 */
int clay_statement_get_line(osl_statement_p statement, int column) {
  osl_relation_p scattering = statement->scattering;
  if (column < 0 || column > scattering->nb_output_dims)
    return -1;
  int i;
  int precision = scattering->precision;
  for (i = 0 ; i < scattering->nb_rows ; i++) {
    if (!osl_int_zero(precision, scattering->m[i], column+1)) {
      break;
    }
  }
  return (i == scattering->nb_rows ? -1 : i );
}