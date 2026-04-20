/***********************************************************************/
/*                                                                     */
/*  a65816_OMF.h : Header pour la gestion du fichier OMF.              */
/*                                                                     */
/***********************************************************************/
/*  Auteur : Olivier ZARDINI  *  Brutal Deluxe Software  *  Janv 2011  */
/***********************************************************************/

#define CRECORD_SIZE                7       /* Taille d'un CRecord */
#define END_SIZE                    1       /* Taille d'un END */

#define PROJECT_TYPE_MONO_FILE_FIXED_ADDR    1      /* [LINK/DIRECT] Fixed Address   / Mono File   / Multi Segment */
#define PROJECT_TYPE_MULTI_FILE_FIXED_ADDR   2      /* [LINK]        Fixed Address   / Multi Files / Multi Segment */
#define PROJECT_TYPE_MONO_SEG_RELOC_ADDR     3      /* [LINK/DIRECT] Relocatable OMF / Mono File   / Mono Segment */
#define PROJECT_TYPE_MULTI_SEG_RELOC_ADDR    4      /* [LINK]        Relocatable OMF / Mono File   / Multi Segment */

#define ADDRESS_TYPE_FIXED                   1      /* Fixed Address */
#define ADDRESS_TYPE_RELATIVE_OMF            2      /* Relative Address for OMF */

struct omf_project
{
  int has_type;            /* le [LINK:TYP] a été trouvéz dans le fichier Link */
  BYTE type;               /* [LINK:TYP] Type du fichier ($B2-$BD pour les OMF) */
  WORD aux_type;           /* [LINK:AUX] AuxType du fichier */
  int express_load;        /* [LINK:XPL] Ajoute le Segment ExpressLoad au début pour les OMF (si le nombre de Segment > 1) */

  /** Liste des Fichiers Multi-Segments (OMF ou Fixed Address) **/
  int nb_file;             /* [LINK:#DSK] Nombre de fichiers pour les Multi-Segment Fixed-Address OneBinaryFile */
  struct omf_file *first_file;
  struct omf_file *last_file;
  
  /** Data du Projet **/
  DWORD project_buffer_length;
  unsigned char *project_buffer_file;

  /* Taille du fichier Projet */
  DWORD project_file_length;

  /* Type de Program */
  int project_type;        /* PROJECT_TYPE_XXX */
  int address_type;        /* ADDRESS_TYPE_XXX : FIXED / RELATIVE_OMF */
  int merge_fa_segments;   /* On colle tous les segments Fixed-Address ensemble, les uns derrière les autres (dans 1 ou plusieurs fichiers) */
};

struct omf_file
{
  char *dsk_path;          /* [LINK:DSK]  Chemin complet du fichier à créer */
  char *dsk_name;          /* [LINK:DSK]  Nom du fichier à créer (en OMF ou en Multi-Segment Fixed-Address SingleBinary) */
  DWORD org_address;       /* [LINK:ORG]  Pour les Multi-Segment Fixed-Address OneBinaryFile, le ORG est fixé par le fichier LINK */
  DWORD file_size;         /* Taille des fichiers pour les Multi-Segment Fixed-Address OneBinaryFile */

  /** Liste des Segments OMF / Fixed Address **/
  int nb_segment;          /* [LINK:#ASM] */
  struct omf_segment *first_segment;
  struct omf_segment *last_segment;

  struct omf_file *next;
};

#define ALIGN_BANK   2
#define ALIGN_PAGE   1
#define ALIGN_NONE   0

struct omf_segment
{
  char *master_file_path;  /* [LINK:ASM] Chemin du fichier Source Master */

  /*****************************************************/
  /*  Valeurs utilisées dans le Header du Segment OMF  */
  /*****************************************************/
  WORD type_attributes;    /* [LINK:KND] Type + Attributs */
  int bank_size;           /* [LINK:BSZ] Bank Size (64KB for code, 0-64 KB for Data, O=can cross boundaries) */
  int org;                 /* Absolute address to load the segment, 0=anywhere */
  int alignment;           /* [LINK:ALI] Boundary Alignement */
  int ds_end;              /* [LINK:DS]  Nombre de 0 à ajouter à la fin du Segment */

  char *load_name;         /* [LINK:LNA] Load Name */
  char *segment_name;      /* [LINK:SNA] Segment Name */

  int file_number;         /* Numéro du fichier (pour les Fixed-Address Single-Binary) */

  /***************************************************/
  /*  Valeurs utilisées dans le Body du Segment OMF  */
  /***************************************************/
  /* Numéro du Segment */
  int segment_number;      /* 1-N */

   /* Pour les Multi-Segment Fixed-Address OneBinaryFile, le ORG est fixé par le fichier LINK */
  int has_org_address;
  DWORD org_address;

  /* Type de fichier en sortie : OMF ou Binaire */
  int is_relative;         /* On a un REL => Pas de Direct Page pour les Label (sauf si l'assemblage est géré via un Link.txt Fixed Address) */

  /** Liste des addresses à patcher **/
  int nb_address;
  struct relocate_address *first_address;
  struct relocate_address *last_address;

  /* Object code */
  int object_length;
  unsigned char *object_code;

  /*** Données du Segment : Header + Body ***/
  DWORD header_length;
  unsigned char segment_header_file[1024];                       /* On prend large */

  DWORD segment_body_length;                                     /* Taille de la zone allouée */
  DWORD body_length;
  unsigned char *segment_body_file;

  /* Header stocké dans l'ExpressLoad */
  DWORD xpress_data_offset;
  DWORD xpress_data_length;
  DWORD xpress_reloc_offset;
  DWORD xpress_reloc_length;

  DWORD header_xpress_length;
  unsigned char header_xpress_file[1024];                        /* On prend large */

  /************************************************/
  /*  Ensemble des structures de données mémoire  */
  /************************************************/
  void *alloc_table[1024];
  struct source_file *first_file;         /* Premier fichier source */
  int nb_opcode;                          /* Liste des opcode */
  struct item *first_opcode;
  struct item *last_opcode;
  struct item **tab_opcode;
  int nb_data;                            /* Liste des data */
  struct item *first_data;
  struct item *last_data;
  struct item **tab_data;
  int nb_directive;                       /* Liste des directive */
  struct item *first_directive;
  struct item *last_directive;
  struct item **tab_directive;
  int nb_direqu;                          /* Liste des directive equivalence */
  struct item *first_direqu;
  struct item *last_direqu;
  struct item **tab_direqu;
  struct item local_item;
  struct item *local_item_ptr;
  int nb_macro;                           /* Liste des macro */
  struct macro *first_macro;
  struct macro *last_macro;
  struct macro **tab_macro;
  struct macro local_macro;
  struct macro *local_macro_ptr;
  int nb_label;                           /* Liste des label */
  struct label *first_label;
  struct label *last_label;
  struct label **tab_label;
  struct label local_label;
  struct label *local_label_ptr;
  int nb_equivalence;                     /* Liste des equivalence */
  struct equivalence *first_equivalence;
  struct equivalence *last_equivalence;
  struct equivalence **tab_equivalence;
  struct equivalence local_equivalence;
  struct equivalence *local_equivalence_ptr;
  int nb_variable;                        /* Liste des variable */
  struct variable *first_variable;
  struct variable *last_variable;
  struct variable **tab_variable;
  struct variable local_variable;
  struct variable *local_variable_ptr;
  int nb_external;                        /* Liste des external EXT */
  struct external *first_external;
  struct external *last_external;
  struct external **tab_external;
  struct external local_external;
  struct external *local_external_ptr;
  int nb_global;                          /* Liste des global ENT */
  struct global *first_global;
  struct global *last_global;
  
  struct omf_segment *next;
};

DWORD BuildOMFHeader(struct omf_project *,struct omf_segment *);
DWORD BuildOMFBody(struct omf_project *,struct omf_segment *);
void RelocateExternalFixedAddress(struct omf_project *,struct omf_segment *);
int BuildOMFFile(char *,struct omf_project *);
int BuildExpressLoadSegment(struct omf_project *);
void UpdateFileInformation(char *,char *,struct omf_project *);
void mem_free_omfproject(struct omf_project *);
struct omf_file *mem_alloc_omffile(char *);
void mem_free_omffile(struct omf_file *);
struct omf_segment *mem_alloc_omfsegment(char *);
void mem_free_omfsegment(struct omf_segment *);

/***********************************************************************/
