#include "vgr_world.h"

#include <assert.h>
#include <string.h>

#include "vgr_dice_throw.h"
#include "vgr_die_modifier.h"
#include "vgr_hex_coordinate.h"
#include "vgr_memory.h"
#include "vgr_strarray.h"
#include "vgr_string.h"
#include "vgr_trade_classification.h"


struct _vgr_world
{
  SF_OBJECT_FIELDS;
  sf_string_t name;
  struct vgr_hex_coordinate hex_coordinate;
  char starport;
  bool naval_base;
  bool scout_base;
  bool gas_giant;
  int size;
  int atmosphere;
  int hydrographics;
  int population;
  int government;
  int law_level;
  int tech_level;
  struct vgr_trade_classification const **trade_classifications;
  int trade_classifications_count;
};


static char *
alloc_trade_classification_abbreviation(void const *item);

static char *
alloc_trade_classification_name(void const *item);

static char *
alloc_trade_classification_short_name(void const *item);

static char
base_code(vgr_world_t world);

static void
dealloc(sf_any_t self);

static sf_string_t 
string_from(sf_any_t self);

static char
hex_digit(int value);


static bool gas_giant_table[] = {
  false,
  false,
  true,  // 2
  true,  // 3
  true,  // 4
  true,  // 5
  true,  // 6
  true,  // 7
  true,  // 8
  true,  // 9
  false, // 10
  false, // 11
  false, // 12
};

static bool naval_base_table[] = {
  false,
  false,
  false, // 2
  false, // 3
  false, // 4
  false, // 5
  false, // 6
  false, // 7
  true,  // 8
  true,  // 9
  true,  // 10
  true,  // 11
  true,  // 12
};

static bool scout_base_table[] = {
  false,
  false,
  false, // 2
  false, // 3
  false, // 4
  false, // 5
  false, // 6
  true,  // 7
  true,  // 8
  true,  // 9
  true,  // 10
  true,  // 11
  true,  // 12
};

static int tech_level_atmosphere_table[] = {
  +1, // 0
  +1, // 1
  +1, // 2
  +1, // 3
   0, // 4
   0, // 5
   0, // 6
   0, // 7
   0, // 8
   0, // 9
  +1, // 10
  +1, // 11
  +1, // 12
  +1, // 13
  +1, // 14
};

static int tech_level_government_table[] = {
  +1, // 0
   0, // 1
   0, // 2
   0, // 3
   0, // 4
  +1, // 5
   0, // 6
   0, // 7
   0, // 8
   0, // 9
   0, // 10
   0, // 11
   0, // 12
  -2, // 13
   0, // 14
   0, // 15
};

static int tech_level_hydrographics_table[] = {
   0, // 0
   0, // 1
   0, // 2
   0, // 3
   0, // 4
   0, // 5
   0, // 6
   0, // 7
   0, // 8
  +1, // 9
  +2, // 10
};

static int tech_level_population_table[] = {
   0, // 0
  +1, // 1
  +1, // 2
  +1, // 3
  +1, // 4
  +1, // 5
   0, // 6
   0, // 7
   0, // 8
  +2, // 9
  +4, // 10
};

static int tech_level_size_table[] = {
  +2, // 0
  +2, // 1
  +1, // 2
  +1, // 3
  +1, // 4
   0, // 5
   0, // 6
   0, // 7
   0, // 8
   0, // 9
   0, // 10
};

static char *starport_table = "??AAABBCCDEEX";

sf_type_t vgr_world_type;


void
_vgr_world_init(void)
{
  vgr_world_type = sf_type("vgr_world_t", dealloc, string_from, NULL, NULL, NULL, NULL);
}


static char *
alloc_trade_classification_abbreviation(void const *item)
{
  struct vgr_trade_classification const *const *trade_classification = item;
  return vgr_strdup((*trade_classification)->abbreviation);
}


static char *
alloc_trade_classification_name(void const *item)
{
  struct vgr_trade_classification const *const *trade_classification = item;
  return vgr_strdup((*trade_classification)->name);
}


static char *
alloc_trade_classification_short_name(void const *item)
{
  struct vgr_trade_classification const *const *trade_classification = item;
  return vgr_strdup((*trade_classification)->short_name);
}


static char
base_code(vgr_world_t world)
{
  if (world->naval_base) {
    return world->scout_base ? 'A' : 'N'; // base code 'A' from Supplement 10: The Solomani Rim
  } else if (world->scout_base) {
    return 'S';
  } else {
    return ' ';
  }
}


static void
dealloc(sf_any_t self)
{
  vgr_world_t world = self;
  
  sf_release(world->name);
  vgr_free(world->trade_classifications);
}


static sf_string_t 
string_from(sf_any_t self)
{
  vgr_world_t world = self;
  
  char *hex_coordinate = vgr_string_alloc_from_hex_coordinate(world->hex_coordinate);
  
  int const max_name_length = 18;
  int const max_classifications_length = 42;
  char const separator[] = ". ";
  
  struct vgr_strarray *classification_names = vgr_strarray_alloc_collect_strings(world->trade_classifications,
                                                                                 world->trade_classifications_count,
                                                                                 sizeof world->trade_classifications[0],
                                                                                 alloc_trade_classification_name);
  char *classifications = vgr_string_alloc_join_strarray_with_suffix(classification_names,
                                                                     separator);
  vgr_strarray_free(classification_names);
  
  if (strlen(classifications) > max_classifications_length) {
    vgr_free(classifications);
    struct vgr_strarray *classification_short_names = vgr_strarray_alloc_collect_strings(world->trade_classifications,
                                                                                         world->trade_classifications_count,
                                                                                         sizeof world->trade_classifications[0],
                                                                                         alloc_trade_classification_short_name);
    classifications = vgr_string_alloc_join_strarray_with_suffix(classification_short_names,
                                                                 separator);
    vgr_strarray_free(classification_short_names);
    
    if (strlen(classifications) > max_classifications_length) {
      vgr_free(classifications);
      struct vgr_strarray *classification_abbreviations = vgr_strarray_alloc_collect_strings(world->trade_classifications,
                                                                                             world->trade_classifications_count,
                                                                                             sizeof world->trade_classifications[0],
                                                                                             alloc_trade_classification_abbreviation);
      classifications = vgr_string_alloc_join_strarray_with_suffix(classification_abbreviations,
                                                                   separator);
      vgr_strarray_free(classification_abbreviations);
    }
  }
  
  sf_string_t string = sf_string_from_format("%*s %4s %c%c%c%c%c%c%c-%c %c %*s%c",
      -max_name_length, sf_string_chars(world->name), hex_coordinate,
      world->starport,
      hex_digit(world->size), hex_digit(world->atmosphere),
      hex_digit(world->hydrographics), hex_digit(world->population),
      hex_digit(world->government), hex_digit(world->law_level),
      hex_digit(world->tech_level), base_code(world),
      -max_classifications_length, classifications,
      (world->gas_giant ? 'G' : ' ')
  );
  
  vgr_free(classifications);
  vgr_free(hex_coordinate);
  return string;
}


static char
hex_digit(int value)
{
  assert(value >= 0);
  assert(value <= 34);

  if (value < 10) {
    return (char) ('0' + value);
  } else {
    return (char) ('A' + value - 10);
  }
}


vgr_world_t
vgr_world(sf_string_t name,
          struct vgr_hex_coordinate const hex_coordinate,
          sf_random_t random_in,
          sf_random_t *random_out)
{
  assert(random_out);
  
  struct _vgr_world *world = sf_object_calloc(sizeof(struct _vgr_world), vgr_world_type);
  if ( ! world) return NULL;
  
  world->name = sf_retain(name);
  world->hex_coordinate = hex_coordinate;
  
  sf_random_t random = random_in;
  sf_list_t modifiers = NULL;
  vgr_dice_throw_t dice_throw = NULL;
  
  dice_throw = vgr_dice_throw(2, 6, NULL, random, &random);
  int starport_index = vgr_dice_throw_total(dice_throw);
  world->starport = starport_table[starport_index];
  
  if ('A' == world->starport || 'B' == world->starport) {
    dice_throw = vgr_dice_throw(2, 6, NULL, random, &random);
    int naval_base_index = vgr_dice_throw_total(dice_throw);
    world->naval_base = naval_base_table[naval_base_index];
  }
  
  if ('E' != world->starport && 'X' != world->starport) {    
    modifiers = NULL;
    if ('C' == world->starport) {
      modifiers = sf_list(vgr_die_modifier(-1), NULL);
    } else if ('B' == world->starport) {
      modifiers = sf_list(vgr_die_modifier(-2), NULL);
    } else if ('A' == world->starport) {
      modifiers = sf_list(vgr_die_modifier(-3), NULL);
    }
    dice_throw = vgr_dice_throw(2, 6, modifiers, random, &random);
    int scout_base_index = vgr_dice_throw_total(dice_throw);
    world->scout_base = scout_base_table[scout_base_index];
  }
  
  dice_throw = vgr_dice_throw(2, 6, NULL, random, &random);
  int gas_giant_index = vgr_dice_throw_total(dice_throw);
  world->gas_giant = gas_giant_table[gas_giant_index];
  
  modifiers = sf_list(vgr_die_modifier(-2), NULL);
  dice_throw = vgr_dice_throw(2, 6, modifiers, random, &random);
  world->size = vgr_dice_throw_total(dice_throw);
  
  if (0 == world->size) {
    world->atmosphere = 0;
  } else {
    modifiers = sf_list_from_items(vgr_die_modifier(-7), vgr_die_modifier(world->size));
    dice_throw = vgr_dice_throw(2, 6, modifiers, random, &random);
    world->atmosphere = vgr_dice_throw_total(dice_throw);
    if (world->atmosphere < 0) world->atmosphere = 0;
  }
  
  if (0 == world->size) {
    world->hydrographics = 0;
  } else {
    modifiers = sf_list_from_items(vgr_die_modifier(-7), vgr_die_modifier(world->atmosphere));
    if (0 == world->atmosphere || 1 == world->atmosphere || world->atmosphere >= 10) {
      modifiers = sf_list(vgr_die_modifier(-4), modifiers);
    }
    dice_throw = vgr_dice_throw(2, 6, modifiers, random, &random);
    world->hydrographics = vgr_dice_throw_total(dice_throw);
    if (world->hydrographics < 0) world->hydrographics = 0;
    if (world->hydrographics > 10) world->hydrographics = 10;
  }
  
  modifiers = sf_list(vgr_die_modifier(-2), NULL);
  dice_throw = vgr_dice_throw(2, 6, modifiers, random, &random);
  world->population = vgr_dice_throw_total(dice_throw);
  
  if (0 == world->population) {
    world->government = 0;
  } else {
    modifiers = sf_list_from_items(vgr_die_modifier(-7), vgr_die_modifier(world->population));
    dice_throw = vgr_dice_throw(2, 6, modifiers, random, &random);
    world->government = vgr_dice_throw_total(dice_throw);
    if (world->government < 0) world->government = 0;
  }
  
  if (0 == world->population) {
    world->law_level = 0;
  } else {
    modifiers = sf_list_from_items(vgr_die_modifier(-7), vgr_die_modifier(world->government));
    dice_throw = vgr_dice_throw(2, 6, modifiers, random, &random);
    world->law_level = vgr_dice_throw_total(dice_throw);
    if (world->law_level < 0) world->law_level = 0;
  }
  
  // TODO: adjust starport if 0 == population
  
  if (world->population) {
    modifiers = NULL;
    
    if ('A' == world->starport) {
      modifiers = sf_list(vgr_die_modifier(+6), modifiers);
    } else if ('B' == world->starport) {
      modifiers = sf_list(vgr_die_modifier(+4), modifiers);
    } else if ('C' == world->starport) {
      modifiers = sf_list(vgr_die_modifier(+2), modifiers);
    } else if ('X' == world->starport) {
      modifiers = sf_list(vgr_die_modifier(-4), modifiers);
    }
    
    modifiers = sf_list(vgr_die_modifier(tech_level_size_table[world->size]), modifiers);
    modifiers = sf_list(vgr_die_modifier(tech_level_atmosphere_table[world->atmosphere]), modifiers);
    modifiers = sf_list(vgr_die_modifier(tech_level_hydrographics_table[world->hydrographics]), modifiers);
    modifiers = sf_list(vgr_die_modifier(tech_level_population_table[world->population]), modifiers);
    modifiers = sf_list(vgr_die_modifier(tech_level_government_table[world->government]), modifiers);
    
    dice_throw = vgr_dice_throw(1, 6, modifiers, random, &random);
    world->tech_level = vgr_dice_throw_total(dice_throw);
    if (world->tech_level < 0) world->tech_level = 0;
  }
  
  world->trade_classifications = vgr_world_alloc_trade_classifications(world, &world->trade_classifications_count);
  
  *random_out = random;
  return sf_move_to_temp_pool(world);
}


int
vgr_world_atmosphere(vgr_world_t world)
{
  return world ? world->atmosphere : 0;
}


int
vgr_world_government(vgr_world_t world)
{
  return world ? world->government : 0;
}


struct vgr_hex_coordinate
vgr_world_hex_coordinate(vgr_world_t world)
{
  return world ? world->hex_coordinate : (struct vgr_hex_coordinate) { .horizontal=0, .vertical=0, };
}


int
vgr_world_hydrographics(vgr_world_t world)
{
  return world ? world->hydrographics : 0;
}


int
vgr_world_law_level(vgr_world_t world)
{
  return world ? world->law_level : 0;
}


sf_string_t
vgr_world_name(vgr_world_t world)
{
  return world ? world->name : NULL;
}


int
vgr_world_population(vgr_world_t world)
{
  return world ? world->population : 0;
}


int
vgr_world_size(vgr_world_t world)
{
  return world ? world->size : 0;
}


char
vgr_world_starport(vgr_world_t world)
{
  return world ? world->starport : 'X';
}