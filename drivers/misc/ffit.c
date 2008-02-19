/*
 * First-Fit / Best-Fit
 * Version 0.2
 *
 * Written by Miguel Masmano Tello <mmasmano@disca.upv.es>
 * Best-Fit strategy implemented by Ismael Ripoll <iripoll@disca.upv.es>
 *
 * Thanks to Ismael Ripoll for his suggestions and reviews
 *
 * Copyright (C) April 2004
 *
 * This code is released using a dual license strategy: GPL/LGPL
 * You can choose the license that better fits your requirements.
 *
 * Released under the terms of the GNU General Public License Version 2.0
 * Released under the terms of the GNU Lesser General Public License Version 2.1
 *
 */


#include "ffit.h"
#include <linux/slab.h>
#include <linux/stddef.h>

/////////////////////////////////////////////////////////////////////////////
// Once SANITY_CHECK is enabled, following functions can be used: memory_dump,
// show_structure, and so on.
//#define SANITY_CHECK
//////////////////////////////////////////////////////////////////////////////


// Some trivial definitions like NULL and printf
#ifndef NULL
   #define NULL ((void *)0)
#endif

#include <linux/kernel.h>
#define ERRMSG( fmt, ... ) printk( KERN_ERR fmt, ## __VA_ARGS__ )


list_header_t *init_memory_pool(unsigned int size, char *ptr) {
   list_header_t *list = kmalloc( sizeof(list_header_t), GFP_KERNEL );
   list->head = (list_t *) ptr;
#ifdef SANITY_CHECK
   list->first_block = list->head;
   list->head -> mn = MN;
#endif

   list->HEADER_SIZE = (unsigned int)list->head->mem.ptr - (unsigned int) list->head;
   size = size & (~0x3);
   list->MIN_SIZE    = 16 ;

   if (list->HEADER_SIZE > list->MIN_SIZE)
   {
      list->MIN_SIZE = ( ( ( list->HEADER_SIZE + 15 ) / 16 ) * 16 );
   }

   list->head -> size = size - list->HEADER_SIZE;
   SET_FREE_BLOCK (list->head);
   SET_LAST_BLOCK (list->head);
   list->head->mem.free_ptr.next = NULL;
   list->head->mem.free_ptr.prev = NULL;
   list->head->prev_phys = NULL;

   return list ;
}


// No matter if it is used
void destroy_memory_pool(list_header_t *list){
   kfree( list );
}

void *rtl_malloc( list_header_t *list, unsigned int size ) {
   list_t *aux, *pos = NULL, *new, *bh3;
   unsigned int new_size;

   if (!size) return (void *) NULL;
   if (size < list->MIN_SIZE) size = list->MIN_SIZE;

   // Rounding up the requested size
   size = ( ( size + 15 ) / 16 ) * 16 ;

   aux = list->head;

#ifdef BESTFIT
   for ( ; aux ;  aux = aux -> mem.free_ptr.next)
   {
      if (GET_BLOCK_SIZE (aux) >= size)
      {
         if (!pos || (GET_BLOCK_SIZE (pos) > GET_BLOCK_SIZE(aux)))
         {
            pos = aux;
         }
         if (GET_BLOCK_SIZE(pos) == size)
            break;
      }
   }
#else // FIRST_FIST
   for ( ; aux ;  aux = aux -> mem.free_ptr.next)
   {
      if (GET_BLOCK_SIZE(aux) >= size)
      {
         pos = aux;
         break;
      }
   }
#endif  // BESTFIT

   aux = pos;

   if (!aux) return (void *) NULL;

   if (aux -> mem.free_ptr.next)
      aux -> mem.free_ptr.next -> mem.free_ptr.prev = aux -> mem.free_ptr.prev;

   if (aux -> mem.free_ptr.prev)
      aux -> mem.free_ptr.prev -> mem.free_ptr.next = aux -> mem.free_ptr.next;

   if (list->head == aux)
      list->head = aux -> mem.free_ptr.next;

   SET_USED_BLOCK (aux);

   aux -> mem.free_ptr.next = NULL;
   aux -> mem.free_ptr.prev = NULL;

   new_size = GET_BLOCK_SIZE(aux) - size - list->HEADER_SIZE;
   if (((int) new_size) >= (int)list->MIN_SIZE)
   {
      new = (list_t *) (((char *) aux) + (unsigned long)list->HEADER_SIZE + (unsigned long) size);
      new -> size = new_size;

      new -> mem.free_ptr.prev = NULL;
      new -> mem.free_ptr.next = NULL;

      SET_FREE_BLOCK (new);

      new -> prev_phys = aux;
      if (IS_LAST_BLOCK(aux))
      {
         SET_LAST_BLOCK (new);
      }
      else
      {
         // updating prev_phys pointer
         bh3 = (list_t *)((char *) new + (unsigned long)list->HEADER_SIZE + (unsigned long) GET_BLOCK_SIZE(new));
         bh3 -> prev_phys = new;
      }

      aux -> size = size;
      SET_USED_BLOCK (aux);

      // the new block is indexed inside of the list of free blocks
      new -> mem.free_ptr.next = list->head;
      if (list->head)
         list->head -> mem.free_ptr.prev = new;
      list->head = new;

   }
   return (void *)aux->mem.ptr ;
}

void rtl_free(list_header_t *list, void *ptr) {
   list_t *b = (list_t *) ((char *)ptr - list->HEADER_SIZE), 
          *b2, 
          *b3;
   if (!ptr)
   {
      ERRMSG ("FREE ERROR: ptr cannot be null\n");
      return;
   }

#ifdef SANITY_CHECK
   check_list ("Entrando free");
   if (b -> mn != MN)
   {
      PRINTF ("ERROR MN 1\n");
      PRINTF ("size ->%d\n", b -> size);
      return;
   }
#endif
   if (!IS_USED_BLOCK(b))
   {
      ERRMSG ("You are releasing a previously released block\n");
      return;
   }
   SET_FREE_BLOCK (b);
   b -> mem.free_ptr.next = NULL;
   b -> mem.free_ptr.prev = NULL;
   if (b -> prev_phys)
   {
      b2 = b -> prev_phys;
      if (!IS_USED_BLOCK (b2))
      {
#ifdef SANITY_CHECK
         list->blocks --;
#endif
         b2 -> size = GET_BLOCK_SIZE(b2) + GET_BLOCK_SIZE (b) + list->HEADER_SIZE;
         if (b2 -> mem.free_ptr.next)
            b2 -> mem.free_ptr.next -> mem.free_ptr.prev = b2 -> mem.free_ptr.prev;

         if (b2 -> mem.free_ptr.prev)
            b2 -> mem.free_ptr.prev -> mem.free_ptr.next = b2 -> mem.free_ptr.next;

         // remove b2 from free list (added below)

         if( list->head == b2)
            list->head = b2 -> mem.free_ptr.next;

         SET_FREE_BLOCK (b2);
         b2 -> mem.free_ptr.next = NULL;
         b2 -> mem.free_ptr.prev = NULL;
         if (IS_LAST_BLOCK (b))
         {
            SET_LAST_BLOCK (b2);
         }
         else
         {
            b3 = (list_t *) (((char *) b2) + (unsigned long) list->HEADER_SIZE + 
                             (unsigned long) GET_BLOCK_SIZE (b2));
            b3 -> prev_phys = b2;
         }
         b = b2;
      } // collapse with predecessor
   }
   if (!IS_LAST_BLOCK (b))
   {
      b2 = (list_t *) (((char *) b) + (unsigned long) list->HEADER_SIZE + 
                       (unsigned long) GET_BLOCK_SIZE (b));

      if (!IS_USED_BLOCK (b2))
      {
         b -> size += GET_BLOCK_SIZE(b2) + list->HEADER_SIZE;

         if (b2 -> mem.free_ptr.next)
            b2 -> mem.free_ptr.next -> mem.free_ptr.prev = b2 -> mem.free_ptr.prev;

         if (b2 -> mem.free_ptr.prev)
            b2 -> mem.free_ptr.prev -> mem.free_ptr.next = b2 -> mem.free_ptr.next;

         if (list->head == b2)
            list->head = b2 -> mem.free_ptr.next;
         b2 -> mem.free_ptr.next = NULL;
         b2 -> mem.free_ptr.prev = NULL;

         if (IS_LAST_BLOCK (b2))
         {
            SET_LAST_BLOCK (b);
         }
         else
         {
            b3 = (list_t *) (((char *) b) + (unsigned long) list->HEADER_SIZE + (unsigned long) GET_BLOCK_SIZE (b));
            b3 -> prev_phys = b;
         }
      }
   }
   b -> mem.free_ptr.next = list->head;

   if (list->head)
      list->head -> mem.free_ptr.prev = b;
   list->head = b;
}
