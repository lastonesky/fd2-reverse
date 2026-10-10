/* world_load_alias.inc - module-local aliases for world_load.c (included from it).
 *
 * port/world_load.c used the IDA short names 'p' (0x53A55) and 'buf' (0x53A61).
 * The shared globals header deliberately omits such collision-prone names
 * (uncertainties §3.5), so they are declared here, only for this module.
 * Types match port/'s own declarations (uint32_t lvalues).
 */
#define p   (*(uint32_t *)&fd2_data1[0x3A55])
#define buf (*(uint32_t *)&fd2_data1[0x3A61])
