/***********************************************************************/
/*                                                                     */
/*  a65816_Link.c : Module d'Assemblage / Linkage d'un source 65c816.  */
/*                                                                     */
/***********************************************************************/
/*  Auteur : Olivier ZARDINI  *  Brutal Deluxe Software  *  Janv 2011  */
/***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <stdint.h>

/** Platform dependent code **/
#if defined(WIN32) || defined(WIN64)
/* Windows */
#else
/* Linux + MacOS */
#include <unistd.h>                     /* unlink() */
#endif

#include "Dc_Library.h"
#include "a65816_Line.h"
#include "a65816_Macro.h"
#include "a65816_OMF.h"
#include "a65816_File.h"
#include "a65816_Lup.h"
#include "a65816_Cond.h"
#include "a65816_Code.h"
#include "a65816_Data.h"
#include "a65816_Link.h"

static int AssembleLink65c816MasterFile(char *,int);
static int AssembleLink65c816LinkFile(struct source_file *,char *,int);
static int AssembleLinkOMFMonoFile(struct omf_project *,int);
static int AssembleLinkFixedAddressMultiFile(struct omf_project *,int);
static int Assemble65c816Segment(struct omf_project *,struct omf_segment *);
static int Link65c816Segment(struct omf_project *, struct omf_segment *);
static int IsLinkFile(struct source_file *);
static struct omf_project *BuildSingleSegment(char *);
static struct omf_project *BuildLinkFile(struct source_file *);
static void GetSourceFileDirective(char *,char *,char *,char *,int);


/**************************************************************************/
/*  AssembleLink65c816() :  Assemble et Links des fichiers source 65c816. */
/**************************************************************************/
int AssembleLink65c816(char *master_file_path, int verbose_mode)
{
  int error, is_link_file;
  char file_name[1024];
  char file_error_path[1024];
  struct source_file *master_link_file;
  struct parameter *param;
  my_Memory(MEMORY_GET_PARAM,&param,NULL,NULL);

  /* Init : Pas de Segment courrant */
  my_Memory(MEMORY_SET_CURR_ERROR_SEGMENT,NULL,NULL,NULL);
  
  /* Reset Fichier <ProjectFolderPath>Error_Output.txt */
  sprintf(file_error_path,"%serror_output.txt",param->project_folder_path);
  unlink(file_error_path);

  /* Extrait le Nom du fichier du chemin */
  GetNameFromPath(master_file_path,file_name);

  /** Charge en mémoire le fichier principal (Master Source ou Link File) **/
  master_link_file = LoadOneSourceFile(master_file_path,file_name,0);
  if(master_link_file == NULL)
    {
      sprintf(param->buffer_error,"Impossible to load Master Source file '%s'",master_file_path);
      my_RaiseError(ERROR_RAISE,param->buffer_error);
    }

  /** Est-ce un fichier Link ou un ficher Source Master ? **/
  is_link_file = IsLinkFile(master_link_file);
  if(is_link_file == 0)
    {
      /** Assemble + Link les fichiers source d'un Master (OMF ou Fixed Address / Mono Segment / Mono File) **/
      error = AssembleLink65c816MasterFile(master_file_path,verbose_mode);
    }
  else
    {
      /** Assemble + Link les fichiers venant d'un Link file (OMF ou Fixed Address / Mono ou Multi Segment / Mono ou Multi File) **/
      error = AssembleLink65c816LinkFile(master_link_file,master_file_path,verbose_mode);
    }

  /* Code Erreur */
  return(error);
}


/************************************************************************************************/
/*  AssembleLink65c816MasterFile() :  Assemble et Links des fichiers d'un Master source 65c816. */
/************************************************************************************************/
static int AssembleLink65c816MasterFile(char *master_file_path, int verbose_mode)
{
  int error;
  struct omf_project *current_omfproject;
  char buffer_typ[1024];
  char buffer_dsk[1024];
  char buffer_org[1024];
  char buffer_name[1024];
  struct parameter *param;
  my_Memory(MEMORY_GET_PARAM,&param,NULL,NULL);

  /** Création d'1 OMF Project + 1 OMF File + 1 OMF Segment **/
  current_omfproject = BuildSingleSegment(master_file_path);
  if(current_omfproject == NULL)
    {
      sprintf(param->buffer_error,"Impossible to allocate memory to process Master Source file '%s'",master_file_path);
      my_RaiseError(ERROR_RAISE,param->buffer_error);
    }

  /** Recherche les Directives TYP / DSK / ORG dans les sources **/
  GetSourceFileDirective(master_file_path,buffer_typ,buffer_dsk,buffer_org,1);

  /* TYP Directive */
  if(strlen(buffer_typ) == 0)
    {
      printf("  Error, Can't find output file Type in Source files (use TYP directive).\n");
      mem_free_omfproject(current_omfproject);
      return(1);
    }
  current_omfproject->type = GetByteValue(buffer_typ);
  current_omfproject->has_type = 1;

  /** Project Type **/
  if(current_omfproject->type >= 0xB2 && current_omfproject->type <= 0xBD)
    {
      /* OMF v2.1 / Mono Segment / Mono File */
      current_omfproject->project_type      = PROJECT_TYPE_MONO_SEG_RELOC_ADDR;
      current_omfproject->address_type      = ADDRESS_TYPE_RELATIVE_OMF;
      current_omfproject->merge_fa_segments = 0;
    }
  else
    {
      /* Fixed Address / Mono Segment / Mono File */
      current_omfproject->project_type      = PROJECT_TYPE_MONO_FILE_FIXED_ADDR;
      current_omfproject->address_type      = ADDRESS_TYPE_FIXED;
      current_omfproject->merge_fa_segments = 0;
      current_omfproject->express_load      = 0;
    }

  /* DSK Directive */
  if(strlen(buffer_dsk) > 0)
    strcpy(buffer_name,buffer_dsk);
  else
    {
      printf("  Error, Can't find output file Name in Source files (use DSK directive).\n");
      mem_free_omfproject(current_omfproject);
      return(1);
    }

  /** Name of the output file **/
  current_omfproject->first_file->dsk_path = BuildProjectFilePath(buffer_name,"DSK");
  if(current_omfproject->first_file->dsk_path == NULL)
    {
      printf("  Error, Impossible to allocate memory for Output File Path.\n");
      mem_free_omfproject(current_omfproject);
      return(1);
    }
  current_omfproject->first_file->dsk_name = BuildProjectFileName(current_omfproject->first_file->dsk_path);
  if(current_omfproject->first_file->dsk_name == NULL)
    {
      printf("  Error, Impossible to allocate memory.\n");
      mem_free_omfproject(current_omfproject);
      return(1);
    }

  /* ORG Directive */
  if(current_omfproject->project_type == PROJECT_TYPE_MONO_FILE_FIXED_ADDR)
    {
      /* Décode la valeur */
      if(strlen(buffer_org) > 0)
        {
          current_omfproject->first_file->org_address = GetDwordValue(buffer_org);
          if(current_omfproject->first_file->org_address > 0xFFFFFF)
            {
              printf("  Error, Invalid ORG value in Source files : %X.\n",(int)current_omfproject->first_file->org_address);
              mem_free_omfproject(current_omfproject);
              return(1);
            }
        }
      else
        current_omfproject->first_file->org_address = 0x000000;     /* Sans ORG dans le Source, on assemble en $00/0000 */
    }
  else
    current_omfproject->first_file->org_address = 0x000000;        /* Pour les OMF, on assemble en $00/0000 */

  /** Assemble + Link Source files **/
  if(current_omfproject->project_type == PROJECT_TYPE_MONO_SEG_RELOC_ADDR)
    error = AssembleLinkOMFMonoFile(current_omfproject,verbose_mode);
  else
    error = AssembleLinkFixedAddressMultiFile(current_omfproject,verbose_mode);

  /* Libération mémoire : OMF Project + OMF File + OMF Segment */
  printf("  + Free ressources...\n");
  mem_free_omfproject(current_omfproject);
  my_Memory(MEMORY_SET_CURR_ERROR_SEGMENT,NULL,NULL,NULL);
  my_File(FILE_CLOSE_DIRECTORY,NULL);

  /* OK */
  return(error);
}


/************************************************************************************************/
/*  AssembleLink65c816LinkFile() :  Assemble et Link des fichiers source 65c816 d'un Link File. */
/************************************************************************************************/
static int AssembleLink65c816LinkFile(struct source_file *link_file, char *master_file_path, int verbose_mode)
{
  int error;
  struct omf_project *current_omfproject;
  struct parameter *param;
  my_Memory(MEMORY_GET_PARAM,&param,NULL,NULL);

  /** Multi Segments : Assemble + Link tous les Segments **/
  printf("  + Loading Link file...\n");

  /** Chargement du fichier Link : 1 OMF Project + N OMF Segment **/
  current_omfproject = BuildLinkFile(link_file);
  if(current_omfproject == NULL)
    {
      mem_free_sourcefile(link_file,1);
      sprintf(param->buffer_error,"Impossible to load Link file '%s'",master_file_path);
      my_RaiseError(ERROR_RAISE,param->buffer_error);
    }

  /* Libération mémoire */
  mem_free_sourcefile(link_file,1);

  /** On Assemble + Link en fonction du type de projet **/
  if(current_omfproject->project_type == PROJECT_TYPE_MONO_SEG_RELOC_ADDR || current_omfproject->project_type == PROJECT_TYPE_MULTI_SEG_RELOC_ADDR)
    error = AssembleLinkOMFMonoFile(current_omfproject,verbose_mode);
  else if(current_omfproject->project_type == PROJECT_TYPE_MONO_FILE_FIXED_ADDR || current_omfproject->project_type == PROJECT_TYPE_MULTI_FILE_FIXED_ADDR)
    error = AssembleLinkFixedAddressMultiFile(current_omfproject,verbose_mode);

  /* Libération mémoire : OMF Project + OMF File + OMF Segment */
  printf("  + Free ressources...\n");
  mem_free_omfproject(current_omfproject);
  my_Memory(MEMORY_SET_CURR_ERROR_SEGMENT,NULL,NULL,NULL);
  my_File(FILE_CLOSE_DIRECTORY,NULL);

  /* OK */
  return(error);
}


/***********************************************************************************************************/
/*  AssembleLinkOMFMonoFile() :  Assemble et Links des fichiers source OMF (Mono/Multi Segment) Mono File. */
/***********************************************************************************************************/
static int AssembleLinkOMFMonoFile(struct omf_project *current_omfproject, int verbose_mode)
{
  int error, segment_number;
  struct omf_segment *current_omfsegment;
  struct omf_file *current_omffile;
  char file_error_path[1024];
  char output_file_path[1024];
  struct parameter *param;
  my_Memory(MEMORY_GET_PARAM,&param,NULL,NULL);

  /* Un seul fichier pour les OMF */
  current_omffile = current_omfproject->first_file;

  /* Numéro du Segment [1-N] : On ajoute l'ExpressLoad s'il y a plus de 1 Segment */
  segment_number = 1;
  if(current_omfproject->express_load == 1 && current_omffile->nb_segment > 1)
    segment_number++;

  /*************************************************************/
  /*** On va enchainer l'assemblage de tous les Segments OMF ***/
  /*************************************************************/
  for(current_omfsegment=current_omffile->first_segment; current_omfsegment; current_omfsegment=current_omfsegment->next)
    {
      /* Init File */
      my_File(FILE_INIT_DIRECTORY,NULL);

      /* Déclare le Segment OMF courrant en cas d'erreur */
      my_Memory(MEMORY_SET_CURR_ERROR_SEGMENT,current_omfsegment,NULL,NULL);

      /* Donne un numéro au Segment [1-N] */
      current_omfsegment->segment_number = segment_number;
      segment_number++;

      /*** Segment #n : Assemble les fichiers Source + Création du fichier Output.txt pour 1 Segment ***/
      printf("  + Assemble project files for Segment #%02X :\n",current_omfsegment->segment_number);
      error = Assemble65c816Segment(current_omfproject,current_omfsegment);
      if(error)
        {
          /* Création du nom du fichier Output Error */
          sprintf(file_error_path,"%s%s_S%02X_%s_Output_Error.txt",param->project_folder_path,current_omffile->dsk_name,current_omfsegment->segment_number,current_omfsegment->segment_name);

          /* Création du fichier Output Error */
          CreateTextOutputFile(file_error_path,current_omfsegment,current_omfproject);
   
          /* Code Erreur */
          return(error);
        }
    }

  /********************************************************************/
  /*** On va générer les Segments OMF (Header + Body + Dictionnary) ***/
  /********************************************************************/
  for(current_omfsegment=current_omffile->first_segment; current_omfsegment; current_omfsegment=current_omfsegment->next)
    {
      /*** Segment #n : Link les fichiers Source pour 1 Segment ***/
      printf("  + Link project files for Segment #%02X...\n",current_omfsegment->segment_number);
      error = Link65c816Segment(current_omfproject,current_omfsegment);
      if(error)
        {
          /* Code Erreur */
          return(error);
        }
    }

  /** Création+Insertion du Segment ~ExpressLoad pour les OMF ayant plus d'un Segment **/
  if(current_omfproject->express_load == 1 && current_omfproject->first_file->nb_segment > 1)
    {
      printf("  + Build ExpressLoad into Segment #01...\n");
      error = BuildExpressLoadSegment(current_omfproject);
      if(error)
        {
          /* Code Erreur */
          return(error);
        }
    }

  /***************************************************/
  /** Create OMF File Mono-Segment / Multi-Segments **/
  /***************************************************/
  printf("  + Build OMF output file...\n");
  BuildOMFFile(param->project_folder_path,current_omfproject);

  /********************************************************/
  /** Dump as Output Text File + Création fichier Symbol **/
  /********************************************************/
  if(verbose_mode)
    {
      /* Création du/des fichiers Output Text */
      printf("  + Create Output Text file%s...\n",(current_omffile->nb_segment == 1)?"":"s");
                        
      /** On va créer les fichiers Output.txt de tous les Segments (sauf l'ExpressLoad) **/
      for(current_omfsegment=current_omffile->first_segment; current_omfsegment; current_omfsegment=current_omfsegment->next)
        {
          /* On ne Dump pas l'ExpressLoad */
          if(current_omfsegment->segment_number == 1 && !my_stricmp(current_omfsegment->segment_name,"~ExpressLoad"))
            continue;

          /* Chemin du fichier _Output.txt */
          sprintf(output_file_path,"%s%s_S%02X_%s_Output.txt",param->project_folder_path,current_omffile->dsk_name,current_omfsegment->segment_number,current_omfsegment->segment_name);

          /* Création du fichier Output */
          CreateTextOutputFile(output_file_path,current_omfsegment,current_omfproject);
        }

      /* Création du fichier Symbols */
      printf("  + Create Symbols file...\n");
      CreateSymbolFile(current_omffile->dsk_name,current_omfproject);
    }

  /* OK */
  return(0);
}


/***********************************************************************************************************************************/
/*  AssembleLinkFixedAddressMultiFile() :  Assemble et Links des fichiers source Fixed Address Mono/Multi file Mono/Multi Segment. */
/***********************************************************************************************************************************/
static int AssembleLinkFixedAddressMultiFile(struct omf_project *current_omfproject, int verbose_mode)
{
  int error, segment_number;
  DWORD org_offset;
  struct omf_segment *current_omfsegment;
  struct omf_file *current_omffile;
  char output_file_path[1024];
  char file_error_path[1024];
  struct parameter *param;
  my_Memory(MEMORY_GET_PARAM,&param,NULL,NULL);

  /* Init */
  segment_number = 1;

  /******************************************************************************/
  /*** On va enchainer l'assemblage de tous les Segments de tous les Fichiers ***/
  /******************************************************************************/
  for(current_omffile=current_omfproject->first_file; current_omffile; current_omffile=current_omffile->next)
    for(org_offset = 0,current_omfsegment=current_omffile->first_segment; current_omfsegment; current_omfsegment=current_omfsegment->next)
      {
        /* Init File */
        my_File(FILE_INIT_DIRECTORY,NULL);

        /* Déclare le Segment OMF courrant (en cas d'erreur) */
        my_Memory(MEMORY_SET_CURR_ERROR_SEGMENT,current_omfsegment,NULL,NULL);

        /* Donne un numéro au Segment [1-N] */
        current_omfsegment->segment_number = segment_number;
        segment_number++;

        /* Donne une adresse ORG pour les fichiers dont les Fixed-Address Segments sont collés les uns aux autres */
        if(current_omfproject->merge_fa_segments == 1)
          {
            current_omfsegment->has_org_address = 1;
            current_omfsegment->org_address = current_omffile->org_address + org_offset;
          }

        /*** Segment #n : Assemble les fichiers Source + Création du fichier Output.txt pour 1 Segment ***/
        printf("  + Assemble project files for Segment #%02X :\n",current_omfsegment->segment_number);
        error = Assemble65c816Segment(current_omfproject,current_omfsegment);
        if(error)
          {
            /* Création du nom du fichier Output Error */
            sprintf(file_error_path,"%s%s_S%02X_%s_Output_Error.txt",param->project_folder_path,current_omffile->dsk_name,current_omfsegment->segment_number,current_omfsegment->segment_name);

            /* Création du fichier Output */
            CreateTextOutputFile(file_error_path,current_omfsegment,current_omfproject);
  
            /* Code Erreur */
            return(error);
          }
            
        /* Met à jour la ORG Address du Segment suivant (pour les Fixed-Address Multi-Segment) */
        org_offset += current_omfsegment->object_length;
      }

  /************************************************/
  /*** On va générer les Segments Fixed Address ***/
  /************************************************/
  for(current_omffile=current_omfproject->first_file; current_omffile; current_omffile=current_omffile->next)
    for(current_omfsegment=current_omffile->first_segment; current_omfsegment; current_omfsegment=current_omfsegment->next)
      {
        /*** Segment #n : Link les fichiers Source pour 1 Segment ***/
        printf("  + Link project files for Segment #%02X...\n",current_omfsegment->segment_number);
        error = Link65c816Segment(current_omfproject,current_omfsegment);
        if(error)
          {  
            /* Code Erreur */
            return(error);
          }
      }

  /***************************************************************/
  /** Create Fixed Address Mono/Multi files Mono/Multi Segments **/
  /***************************************************************/
  printf("  + Build Binary output file%s...\n",(current_omfproject->nb_file==1)?"":"s");

  /** Création du/des fichiers Binaire à adresse fixe (groupés ensemble si multi sgements) **/
  for(current_omffile = current_omfproject->first_file; current_omffile; current_omffile = current_omffile->next)
    BuildFixedAddressBinaryOutputFile(current_omffile,current_omfproject);

  /******************************/
  /** Dump as Output Text File **/
  /******************************/
  if(verbose_mode)
    {
      /** On va créer les fichiers Output.txt de tous les Segments de tous les Fichiers **/
      printf("  + Create Output Text file%s...\n",(segment_number > 2)?"s":"");
      for(current_omffile = current_omfproject->first_file; current_omffile; current_omffile = current_omffile->next)
        for(current_omfsegment=current_omffile->first_segment; current_omfsegment; current_omfsegment=current_omfsegment->next)
          {
            /* Chemin du fichier _Output.txt */
            sprintf(output_file_path,"%s%s_S%02X_%s_Output.txt",param->project_folder_path,current_omffile->dsk_name,current_omfsegment->segment_number,current_omfsegment->segment_name);

            /* Création du fichier Output */
            CreateTextOutputFile(output_file_path,current_omfsegment,current_omfproject);
          }

      /* Création des fichiers Symbols de tous les fichiers */
      printf("  + Create Symbol files...\n");
      for(current_omffile = current_omfproject->first_file; current_omffile; current_omffile = current_omffile->next)
        CreateSymbolFile(current_omffile->dsk_name,current_omfproject);
    }

  /* OK */
  return(0);
}


/*********************************************************************************/
/*  Assemble65c816Segment() :  Assemble des fichiers source 65c816 d'un Segment. */
/*********************************************************************************/
static int Assemble65c816Segment(struct omf_project *current_omfproject, struct omf_segment *current_omfsegment)
{
  int modified, error, first_time, has_error;
  struct source_file *first_file;
  struct source_line *current_line;
  struct relocate_address *current_address;
  struct relocate_address *next_address;
  struct parameter *param;
  my_Memory(MEMORY_GET_PARAM,&param,NULL,NULL);

  /* Init */
  my_Memory(MEMORY_FREE_EQUIVALENCE,NULL,NULL,current_omfsegment);

  /* Initialisation du compteur des labels uniques */
  GetUNID("INIT=1");

  /* Récupère le répertoire des fichiers source */
  GetFolderFromPath(current_omfsegment->master_file_path,param->source_folder_path);

  /* Création des tables de recherche rapides */
  BuildReferenceTable(current_omfsegment);

  /** Chargement de tous les fichiers Source / Identifie les lignes en Commentaire + Vide **/
  printf("    o Loading Sources files...\n");
  LoadAllSourceFile(current_omfsegment->master_file_path,current_omfsegment);

  /** Chargement des fichiers Macro **/
  printf("    o Loading Macro files...\n");
  LoadSourceMacroFile(current_omfsegment);

  /** Recherche des Macro supplémentaires définies directement dans le Source **/
  GetMacroFromSource(current_omfsegment);

  /** Tri toutes les Macro **/
  my_Memory(MEMORY_SORT_MACRO,NULL,NULL,current_omfsegment);

  /** Recherche des Macro en double **/
  printf("    o Check for duplicated Macros...\n");
  CheckForDuplicatedMacro(current_omfsegment);

  /** Détermine le type des lignes (Code, Macro, Directive, Equivalence...) **/
  printf("    o Decoding lines types...\n");
  my_Memory(MEMORY_GET_SOURCE_FILE,&first_file,NULL,current_omfsegment);  
  error = DecodeLineType(first_file->first_line,NULL,current_omfsegment,current_omfproject);
  if(error)
    return(1);

  /** Remplace les labels :locaux/]variable par un unid_ dans le Code et les Macros **/
  printf("    o Process local/variable Labels...\n");
  ProcessAllLocalLabel(current_omfsegment);
  ProcessAllVariableLabel(current_omfsegment);

  /** Remplace les * par des Labels dans le Code et les Macros **/
  printf("    o Process Asterisk lines...\n");
  error = ProcessAllAsteriskLine(current_omfsegment);
  if(error)
    return(1);

  /** Création de la table des External **/
  printf("    o Build External table...\n");
  BuildExternalTable(current_omfsegment);

  /** Création de la table des Equivalences **/
  printf("    o Build Equivalence table...\n");
  BuildEquivalenceTable(current_omfsegment);

  /** Création de la table des variables ]LP **/
  printf("    o Build Variable table...\n");
  BuildVariableTable(current_omfsegment);

  /** Remplace les Equivalences **/
  printf("    o Process Equivalence values...\n");
  ProcessEquivalence(current_omfsegment);

  /** Remplace les Macro avec leur code **/
  printf("    o Replace Macros with Code...\n");
  error = ReplaceMacroWithContent(current_omfsegment,current_omfproject);
  if(error)
    return(1);

  /** Remplace les LUP par la séquence de code (cela vient après les Macro car le param de LUP peut être un des param d'appel de la Macro) **/
  printf("    o Replace Lup with code...\n");
  ReplaceLupWithCode(current_omfsegment);

  /** On refait les Equivalences, les Variables et les Remplacements dans le code amené par les Macro et les Loop **/
  BuildEquivalenceTable(current_omfsegment);
  BuildVariableTable(current_omfsegment);
  ProcessEquivalence(current_omfsegment);

  /** On va traiter les MX **/
  printf("    o Process MX directives...\n");
  ProcessMXDirective(current_omfsegment);

  /** On va traiter les Conditions **/
  printf("    o Process Conditional directives...\n");
  ProcessConditionalDirective(current_omfsegment);

  /** Création de la table des Labels **/
  printf("    o Build Label table...\n");
  BuildLabelTable(current_omfsegment);

  /** Recherche de Labels/Equivalence déclarés plusieurs fois **/
  printf("    o Check for duplicated Labels...\n");
  error = CheckForDuplicatedLabel(current_omfsegment);
  if(error)
    return(1);

  /** Détecte la liste des lignes inconnues **/
  printf("    o Check for unknown Source lines...\n");
  error = CheckForUnknownLine(current_omfsegment);
  if(error)
    return(1);

  /** Traite les lignes Dum **/
  printf("    o Check for Dum lines...\n");
  error = CheckForDumLine(current_omfsegment);
  if(error)
    return(1);

  /**** La détection automatique du Direct Page nous oblige à itérer plusieurs fois ****/
  first_time = 1;
  modified   = 1;
  has_error  = 0;
  while(modified == 1 || has_error == 1)
    {
      /* Erreur lors de la génération du code */
      strcpy(param->buffer_latest_error,"");
      has_error = 0;

      /*** Génération du code Opcode + Calcul de la Taille pour chaque ligne de Code ***/
      if(first_time)
        printf("    o Compute Operand Code size...\n");
      BuildAllCodeLineSize(current_omfsegment);

      /*** Calcul de la taille pour chaque ligne de Data ***/
      if(first_time)
        printf("    o Compute Operand Data size...\n");
      BuildAllDataLineSize(current_omfsegment);

      /*** Calcul les addresses de chaque ligne + Analyse les ORG / OBJ / REL / DUM ***/
      if(first_time)
        printf("    o Compute Line address...\n");
      ComputeLineAddress(current_omfsegment,current_omfproject);

      /*** Génération du binaire pour les lignes Code (LINE_CODE) ***/
      if(first_time)
        printf("    o Build Code Line...\n");
      BuildAllCodeLine(&has_error,current_omfsegment,current_omfproject);

      /** Compact Code for Direct Page (sauf si l'adresse est relogeable OMF ) **/
      if(first_time)
        printf("    o Compact Code for Direct Page Lines...\n");
      modified = CompactDirectPageCode(current_omfsegment);

      /* Le compact de donne rien et on a une erreur => On sort */
      if(has_error == 1 && modified == 0)
        my_RaiseError(ERROR_RAISE,param->buffer_latest_error);

      /** Il faut tout refaire => on va supprimer toutes les adresses à reloger **/
      if(modified == 1 || has_error == 1)
        {
          for(current_address=current_omfsegment->first_address; current_address; )
            {
              next_address = current_address->next;
              free(current_address);
              current_address = next_address;
            }
          current_omfsegment->first_address = NULL;
          current_omfsegment->last_address  = NULL;
          current_omfsegment->nb_address    = 0;
        }

      /* On a déjà fait un tour */
      first_time = 0;
    }

  /** On va évaluer les lignes ERR **/
  printf("    o Check for Err lines...\n");
  error = CheckForErrLine(current_omfsegment);
  if(error)
    return(1);

  /** On va vérifier les adressages Page Direct **/
  printf("    o Check for Direct Page Lines...\n");
  error = CheckForDirectPageLine(current_omfsegment);
  if(error)
    return(1);

  /*** Génération du binaire pour les lignes Data (LINE_DATA) ***/
  printf("    o Build Data Line...\n");
  BuildAllDataLine(current_omfsegment);

  /** Create Object Code (LINE_CODE + LINE_DATA) **/
  printf("    o Build Object Code...\n");
  BuildObjectCode(current_omfsegment);

  /** Transforme les lignes directive avec Label en ligne Vide **/
  ProcessDirectiveWithLabelLine(current_omfsegment);

  /** Si on est en multi-segment Fixed, il faut faire remonter l'org address du segment **/
  if(current_omfproject->address_type == ADDRESS_TYPE_FIXED)
    {
      /* On recherche la première ligne avec qqchose dedans */
      for(current_line=current_omfsegment->first_file->first_line; current_line; current_line=current_line->next)
        if(current_line->nb_byte > 0)
          {
            current_omfsegment->org = current_line->address;
            break;
          }
    }

  /* OK */
  return(0);
}


/**********************************************************************************************/
/*  Link65c816Segment() :  Link les fichiers source 65c816 d'un Segment OMF ou Fixed Address. */
/**********************************************************************************************/
static int Link65c816Segment(struct omf_project *current_omfproject, struct omf_segment *current_omfsegment)
{
  int nb_external;
  struct label *external_label;
  struct relocate_address *current_address;
  struct omf_file *external_omffile;
  struct omf_segment *external_omfsegment;
  struct parameter *param;
  my_Memory(MEMORY_GET_PARAM,&param,NULL,NULL);

  /** On va vérifier que tous les External utilisés de ce segments sont résolus **/
  my_Memory(MEMORY_GET_EXTERNAL_NB,&nb_external,NULL,current_omfsegment);
  if(nb_external > 0)
    {
      /** On vérifie toutes les adresses relogeables **/
      for(current_address = current_omfsegment->first_address; current_address; current_address=current_address->next)
        if(current_address->external != NULL)
          if(current_address->external->external_segment == NULL)
            {
              /** On passe tous les Segments en revue pour trouver le label Global associé à ce External **/
              for(external_omffile = current_omfproject->first_file; external_omffile; external_omffile = external_omffile->next)
                for(external_omfsegment = external_omffile->first_segment; external_omfsegment; external_omfsegment = external_omfsegment->next)
                  {
                    /* Recherche un Label Global */
                    my_Memory(MEMORY_SEARCH_LABEL,current_address->external->name,&external_label,external_omfsegment);
                    if(external_label != NULL)
                      if(external_label->is_global == 1)
                        {
                          /* Si on en a déjà trouvé un, c'est qu'il existe au moins 2 Label Global s'appelant pareil => Erreur */
                          if(current_address->external->external_segment != NULL)
                            {
                              printf("     => Error : We have found 2 External Labels with the same name '%s' (File '%s', Line %d).\n",current_address->external->name,external_label->line->file->file_path,external_label->line->file_line_number);
                              return(1);
                            }
                        
                          /* On conserve celui là */
                          current_address->external->external_segment = external_omfsegment;
                          current_address->external->external_label = external_label;
                        }
                  }
  
              /** On n'a pas pu trouver le Segment contenant ce Label externe :-( **/
              if(current_address->external->external_segment == NULL)
                {
                  printf("     => Error : Can't find External Label named '%s' (File '%s', Line %d).\n",current_address->external->name,current_address->external->source_line->file->file_path,current_address->external->source_line->file_line_number);
                  return(1);
                }            
            }
    }

  /** Taille du Body du Segment (on prend plus large pour l'OMF) **/
  if(current_omfproject->address_type == ADDRESS_TYPE_FIXED)
    current_omfsegment->segment_body_length = current_omfsegment->object_length;
  else
    current_omfsegment->segment_body_length = 1024 + current_omfsegment->object_length + CRECORD_SIZE*current_omfsegment->nb_address + END_SIZE;

  /** Allocation mémoire Segment Body **/
  current_omfsegment->segment_body_file = (unsigned char *) calloc(current_omfsegment->segment_body_length,sizeof(unsigned char));
  if(current_omfsegment->segment_body_file == NULL)
    {
      printf("     => Error : Can't allocate memory to build Body File buffer.\n");
      return(1);
    }

  /** Création du Segment Body **/
  if(current_omfproject->address_type == ADDRESS_TYPE_FIXED)
    {
      /* Reloge les adresses externes fixed */
      RelocateExternalFixedAddress(current_omfproject,current_omfsegment);
      
      /* Conserve le code objet */
      memcpy(current_omfsegment->segment_body_file,current_omfsegment->object_code,current_omfsegment->object_length);
    }
  else
    current_omfsegment->body_length = BuildOMFBody(current_omfproject,current_omfsegment);

  /* Création du Segment Header */
  if(current_omfproject->address_type == ADDRESS_TYPE_FIXED)
    current_omfsegment->header_length = 0;
  else
    current_omfsegment->header_length = BuildOMFHeader(current_omfproject,current_omfsegment);

  /* OK */
  return(0);
}


/************************************************************************/
/*  IsLinkFile() :  Détermine si un fichier Source est un fichier Link. */
/************************************************************************/
static int IsLinkFile(struct source_file *master_file)
{
  int i, nb_link_opcode, nb_total_opcode;
  struct source_line *current_line;
  char *opcode_link[] = {"DSK","TYP","AUX","XPL","ASM","DS","KND","ALI","LNA","SNA","ORG","BSZ",NULL};      /* Opcode exclusifs au fichier Link (sauf DSK et TYP) */

  /* Init */
  nb_link_opcode = 0;
  nb_total_opcode = 0;

  /* Fichier vide ? */
  if(master_file->first_line == NULL)
    return(0);

  /** On passe toutes les lignes en revue **/
  for(current_line = master_file->first_line; current_line; current_line = current_line->next)
    {
      /* Commentaire / Vide */
      if(current_line->type == LINE_COMMENT || current_line->type == LINE_EMPTY)
        continue;

      /* Reconnait les Opcode du Link */
      for(i=0; opcode_link[i] != NULL; i++)
        if(!my_stricmp(current_line->opcode_txt,opcode_link[i]))
          {
            nb_link_opcode++;
            break;
          }
      nb_total_opcode++;
    }

  /* KO */
  if(nb_total_opcode == 0)
    return(0);

  /* OK */
  return((nb_link_opcode == nb_total_opcode) ? 1 : 0);
}


/************************************************************/
/*  BuildSingleSegment() :  Création d'un OMF mono-Segment. */
/************************************************************/
static struct omf_project *BuildSingleSegment(char *master_file_path)
{
  struct omf_project *current_omfproject;
  struct omf_file *current_omffile;
  struct omf_segment *current_omfsegment;
  
  /* OMF Header */
  current_omfproject = (struct omf_project *) calloc(1,sizeof(struct omf_project));
  if(current_omfproject == NULL)
    return(NULL);

  /** 1 OMF File **/
  current_omffile = mem_alloc_omffile("");
  if(current_omffile == NULL)
    {
      mem_free_omfproject(current_omfproject);
      return(NULL);
    }
  current_omfproject->nb_file    = 1;
  current_omfproject->first_file = current_omffile;
  current_omfproject->last_file  = current_omffile;

  /** 1 OMF Segment **/
  current_omfsegment = mem_alloc_omfsegment("Segment1");
  if(current_omfsegment == NULL)
    {
      mem_free_omfproject(current_omfproject);
      return(NULL);
    }
  current_omffile->nb_segment    = 1;
  current_omffile->first_segment = current_omfsegment;
  current_omffile->last_segment  = current_omfsegment;

  /* Chemin du Master Source file */
  current_omfsegment->master_file_path = strdup(master_file_path);
  if(current_omfsegment->master_file_path == NULL)
    {
      mem_free_omfproject(current_omfproject);
      return(NULL);
    }

  /* En mono-Segment OMF, il n'y a pas d'ExpressLoad, le premier segment a le numéro 1 (idem pour les Fixed Address) */
  current_omfsegment->segment_number = 1;  

  /* Renvoi la structure */
  return(current_omfproject);
}


/*************************************************************/
/*  BuildLinkFile() :  Récupère les données du fichier Link. */
/*************************************************************/
static struct omf_project *BuildLinkFile(struct source_file *link_file)
{
  struct omf_project *current_omfproject;
  struct omf_file *current_omffile;
  struct omf_segment *current_omfsegment;
  struct source_line *current_line;
  struct parameter *param;
  my_Memory(MEMORY_GET_PARAM,&param,NULL,NULL);

  /* Allocation mémoire OMF Project */
  current_omfproject = (struct omf_project *) calloc(1,sizeof(struct omf_project));
  if(current_omfproject == NULL)
    {
      printf("     => Error, Impossible to allocate memory to process Link file.\n");
      return(NULL);
    }

  /*****************************************************/
  /*** Passe toutes les lignes du Link File en revue ***/
  /*****************************************************/
  for(current_line = link_file->first_line; current_line; current_line = current_line->next)
    {
      /* Commentaire / Vide */
      if(current_line->type == LINE_COMMENT || current_line->type == LINE_EMPTY)
        continue;

      /********************/
      /** Nouveau Header **/
      /********************/
      /** TYP : Type du fchier **/
      if(!my_stricmp(current_line->opcode_txt,"TYP"))
        {
          /* Décode les Alias Ascii (S16 pour $B3) */
          DecodeDirectiveTYP(current_line->operand_txt);

          /* Décode la valeur */
          current_omfproject->type = GetByteValue(current_line->operand_txt);
          current_omfproject->has_type = 1;
          continue;
        }
        
      /** AUX : AuxType du fichier **/
      if(!my_stricmp(current_line->opcode_txt,"AUX"))
        {
          /* Décode la valeur */
          current_omfproject->aux_type = GetWordValue(current_line->operand_txt);
          continue;
        }
        
      /** XPL : Express Load **/
      if(!my_stricmp(current_line->opcode_txt,"XPL"))
        {
          current_omfproject->express_load = 1;
          continue;
        }
      
      /***************************/
      /** DSK : Nouveau Fichier **/
      /***************************/
      if(!my_stricmp(current_line->opcode_txt,"DSK"))
        {
          /** Nouveau OMF File **/
          current_omffile = mem_alloc_omffile(current_line->operand_txt);
          if(current_omffile == NULL)
            {
              printf("     => Error, Impossible to allocate memory to process Link file.\n");
              mem_free_omfproject(current_omfproject);
              return(NULL);
            }

          /* Un fichier de plus */
          current_omfproject->nb_file++;
          if(current_omfproject->first_file == NULL)
            current_omfproject->first_file = current_omffile;
          else
            current_omfproject->last_file->next = current_omffile;
          current_omfproject->last_file = current_omffile;
          
          /* Ligne suivante */
          continue;
        }

      /** ORG : Adresse d'assemblage du fichier Fixed Address **/
      if(!my_stricmp(current_line->opcode_txt,"ORG"))
        {
          /* Un ORG s'inscrit dans un fichier */
          if(current_omfproject->nb_file == 0)
            {
              printf("     => Error, Invalid ORG directive (%s) in Link file. Missing previous DSK directive.\n",current_line->operand_txt);
              mem_free_omfproject(current_omfproject);
              return(NULL);
            }

          /* Décode la valeur + Vérifie la plage */
          current_omfproject->last_file->org_address = GetDwordValue(current_line->operand_txt);
          if(current_omfproject->last_file->org_address > 0xFFFFFF)
            {
              printf("     => Error, Invalid ORG value in Link file : %X.\n",(int)current_omfproject->last_file->org_address);
              mem_free_omfproject(current_omfproject);
              return(NULL);
            }

          /* Ligne suivante */
          continue;
        }

      /***************************/
      /** ASM : Nouveau Segment **/
      /***************************/
      if(!my_stricmp(current_line->opcode_txt,"ASM"))
        {
          /* Un ASM s'inscrit dans un fichier */
          if(current_omfproject->nb_file == 0)
            {
              printf("     => Error, Invalid ASM directive (%s) in Link file. Missing previous DSK directive.\n",current_line->operand_txt);
              mem_free_omfproject(current_omfproject);
              return(NULL);
            }

          /* Allocation mémoire du Segment */
          current_omfsegment = mem_alloc_omfsegment("");
          if(current_omfsegment == NULL)
            {
              printf("     => Error, Impossible to allocate memory to process Link file.\n");
              mem_free_omfproject(current_omfproject);
              return(NULL);
            }

          /* Attachement du Segment à la liste */
          if(current_omfproject->last_file->first_segment == NULL)
            current_omfproject->last_file->first_segment = current_omfsegment;
          else
            current_omfproject->last_file->last_segment->next = current_omfsegment;
          current_omfproject->last_file->last_segment = current_omfsegment;
          current_omfproject->last_file->nb_segment++;

          /* Chemin du fichier Source Master */
          current_omfsegment->master_file_path = BuildProjectFilePath(current_line->operand_txt,"ASM");
          if(current_omfsegment->master_file_path == NULL)
            {
              printf("     => Error, Impossible to allocate memory to process Link file.\n");
              mem_free_omfproject(current_omfproject);
              return(NULL);
            }

          /* Numéro du fichier (1-N) */
          current_omfsegment->file_number = current_omfproject->nb_file;

          /* Ligne suivante */
          continue;
        }

      /** DS : Nombre de 0 à ajouter à la fin du segment **/
      if(!my_stricmp(current_line->opcode_txt,"DS"))
        {
          /* Un DS s'inscrit dans un File/Segment */
          if(current_omfproject->nb_file == 0)
            {
              printf("     => Error, Invalid DS directive (%s) in Link file. Missing previous DSK directive.\n",current_line->operand_txt);
              mem_free_omfproject(current_omfproject);
              return(NULL);
            }
          if(current_omfproject->last_file->last_segment == NULL)
            {
              printf("     => Error, Invalid DS directive (%s) in Link file. Missing previous SNA directive.\n",current_line->operand_txt);
              mem_free_omfproject(current_omfproject);
              return(NULL);
            }
          current_omfsegment = current_omfproject->last_file->last_segment;

          /* Décode la valeur */
          current_omfproject->last_file->last_segment->ds_end = atoi(current_line->operand_txt);
          continue;
        }

      /** KND : Type et Attributs du Segment **/
      if(!my_stricmp(current_line->opcode_txt,"KND"))
        {
          /* Un KND s'inscrit dans un File/Segment */
          if(current_omfproject->nb_file == 0)
            {
              printf("     => Error, Invalid KND directive (%s) in Link file. Missing previous DSK directive.\n",current_line->operand_txt);
              mem_free_omfproject(current_omfproject);
              return(NULL);
            }
          if(current_omfproject->last_file->last_segment == NULL)
            {
              printf("     => Error, Invalid KND directive (%s) in Link file. Missing previous SNA directive.\n",current_line->operand_txt);
              mem_free_omfproject(current_omfproject);
              return(NULL);
            }
          current_omfsegment = current_omfproject->last_file->last_segment;

          /* Décode la valeur */
          current_omfsegment->type_attributes = GetWordValue(current_line->operand_txt);

          /* Vérifie les valeurs : Type */
          if(!(((current_omfsegment->type_attributes & 0x00FF) == 0x0000) ||
               ((current_omfsegment->type_attributes & 0x00FF) == 0x0001) ||
               ((current_omfsegment->type_attributes & 0x00FF) == 0x0002) ||
               ((current_omfsegment->type_attributes & 0x00FF) == 0x0004) ||
               ((current_omfsegment->type_attributes & 0x00FF) == 0x0008) ||
               ((current_omfsegment->type_attributes & 0x00FF) == 0x0010) ||
               ((current_omfsegment->type_attributes & 0x00FF) == 0x0012)))
            {
              printf("     => Error, Invalid Link file : Unknown Type value for Directive KND (%04X) at line %d.\n",current_omfsegment->type_attributes,current_line->file_line_number);
              mem_free_omfproject(current_omfproject);
              return(NULL);
            }

          /* Valeur suivante */
          continue;
        }

      /** ALI : Alignement **/
      if(!my_stricmp(current_line->opcode_txt,"ALI"))
        {
          /* Un ALI s'inscrit dans un File/Segment */
          if(current_omfproject->nb_file == 0)
            {
              printf("     => Error, Invalid ALI directive (%s) in Link file. Missing previous DSK directive.\n",current_line->operand_txt);
              mem_free_omfproject(current_omfproject);
              return(NULL);
            }
          if(current_omfproject->last_file->last_segment == NULL)
            {
              printf("     => Error, Invalid ALI directive (%s) in Link file. Missing previous SNA directive.\n",current_line->operand_txt);
              mem_free_omfproject(current_omfproject);
              return(NULL);
            }
          current_omfsegment = current_omfproject->last_file->last_segment;

          /* Décode la valeur */
          if(!my_stricmp(current_line->operand_txt,"BANK"))
            current_omfsegment->alignment = ALIGN_BANK;
          else if(!my_stricmp(current_line->operand_txt,"PAGE"))
            current_omfsegment->alignment = ALIGN_PAGE;
          else if(!my_stricmp(current_line->operand_txt,"NONE"))
            current_omfsegment->alignment = ALIGN_NONE;
          else
            {
              printf("     => Error, Invalid Link file : Unknown value for Directive ALI (%s) at line %d.\n",current_line->operand_txt,current_line->file_line_number);
              mem_free_omfproject(current_omfproject);
              return(NULL);
            }

          /* Ligne suivante */
          continue;
        }

      /** LNA : Load Name **/
      if(!my_stricmp(current_line->opcode_txt,"LNA"))
        {
          /* Un LNA s'inscrit dans un File/Segment */
          if(current_omfproject->nb_file == 0)
            {
              printf("     => Error, Invalid LNA directive (%s) in Link file. Missing previous DSK directive.\n",current_line->operand_txt);
              mem_free_omfproject(current_omfproject);
              return(NULL);
            }
          if(current_omfproject->last_file->last_segment == NULL)
            {
              printf("     => Error, Invalid LNA directive (%s) in Link file. Missing previous SNA directive.\n",current_line->operand_txt);
              mem_free_omfproject(current_omfproject);
              return(NULL);
            }
          current_omfsegment = current_omfproject->last_file->last_segment;

          /* Décode la valeur */
          if(strlen(current_omfsegment->load_name) > 0)
            {
              /* On a déjà un LNA */
              if(!my_stricmp(current_omfsegment->load_name,current_line->operand_txt))
                continue;   /* Même valeur, on ne dit rien */
              else
                {
                  printf("     => Error, Invalid Link file : Two LNA directives found for Segment '%s'.\n",current_omfsegment->master_file_path);
                  mem_free_omfproject(current_omfproject);
                  return(NULL);
                }
            }
          else
            {
              /* Nouvelle valeur */
              free(current_omfsegment->load_name);
              current_omfsegment->load_name = strdup(current_line->operand_txt);
              if(current_omfsegment->load_name == NULL)
                {
                  printf("     => Error, Impossible to allocate memory to process Link file.\n");
                  mem_free_omfproject(current_omfproject);
                  return(NULL);
                }

              /* Enlève les " ou ' */
              CleanUpName(current_omfsegment->load_name);
            }

          /* Ligne suivante */
          continue;
        }

      /** SNA : Segment Name **/
      if(!my_stricmp(current_line->opcode_txt,"SNA"))
        {
          /* Un SNA s'inscrit dans un File/Segment */
          if(current_omfproject->nb_file == 0)
            {
              printf("     => Error, Invalid SNA directive (%s) in Link file. Missing previous DSK directive.\n",current_line->operand_txt);
              mem_free_omfproject(current_omfproject);
              return(NULL);
            }
          if(current_omfproject->last_file->last_segment == NULL)
            {
              printf("     => Error, Invalid SNA directive (%s) in Link file. Missing previous SNA directive.\n",current_line->operand_txt);
              mem_free_omfproject(current_omfproject);
              return(NULL);
            }
          current_omfsegment = current_omfproject->last_file->last_segment;

          /* Décode la valeur */
          if(strlen(current_omfsegment->segment_name) > 0)
            {
              /* On a déjà un SNA */
              if(!my_stricmp(current_omfsegment->segment_name,current_line->operand_txt))
                continue;   /* Même valeur, on ne dit rien */
              else
                {
                  printf("     => Error, Invalid Link file : Two SNA directives found for Segment '%s'.\n",current_omfsegment->master_file_path);
                  mem_free_omfproject(current_omfproject);
                  return(NULL);
                }
            }
          else
            {
              /* Nouvelle Valeur */
              free(current_omfsegment->segment_name);
              current_omfsegment->segment_name = strdup(current_line->operand_txt);
              if(current_omfsegment->segment_name == NULL)
                {
                  printf("     => Error, Impossible to allocate memory to process Link file.\n");
                  mem_free_omfproject(current_omfproject);
                  return(NULL);
                }

              /* Enlève les " ou ' */
              CleanUpName(current_omfsegment->segment_name);
            }

          /* Ligne suivante */
          continue;
        }

      /** Commande inconnue **/
      printf("     => Error, Invalid Link file : Unknown directive found (%s) at line %d.\n",current_line->opcode_txt,current_line->file_line_number);
      mem_free_omfproject(current_omfproject);
      return(NULL);
    }

  /*******************************************************/
  /** On valide les valeurs du Project / File / Segment **/
  /*******************************************************/
  /** [DSK] : File **/
  if(current_omfproject->nb_file == 0)
    {
      printf("     => Error, Invalid Link file : No File defined (use DSK directive).\n");
      mem_free_omfproject(current_omfproject);
      return(NULL);
    }

  /** [ASM] : Segment **/
  for(current_omffile = current_omfproject->first_file; current_omffile; current_omffile=current_omffile->next)
    if(current_omffile->nb_segment == 0)
      {
        printf("     => Error, Invalid Link file : No Segment defined (use ASM directive).\n");
        mem_free_omfproject(current_omfproject);
        return(NULL);
      }

  /** [TYP] : Type **/
  if(current_omfproject->has_type == 0)
    {
      printf("     => Error, Invalid Link file : No Type defined (use TYP directive).\n");
      mem_free_omfproject(current_omfproject);
      return(NULL);
    }

  /** [SNA] : On veut des noms de Segment **/
  for(current_omffile = current_omfproject->first_file; current_omffile; current_omffile=current_omffile->next)
    for(current_omfsegment = current_omffile->first_segment; current_omfsegment; current_omfsegment = current_omfsegment->next)
      if(strlen(current_omfsegment->segment_name) == 0)
        {
          printf("     => Error, Invalid Link file : Missing Segment Name for '%s' (use SNA directive).\n",current_omfsegment->master_file_path);
          mem_free_omfproject(current_omfproject);
          return(NULL);
        }

  /** Fichier OMF / Mono ou Multi-Segment / Single File **/
  if(current_omfproject->type >= 0xB2 && current_omfproject->type <= 0xBD)
    {
      /* Les Fichiers OMF sont mono-file */
      if(current_omfproject->nb_file != 1)
        {
          printf("     => Error, Invalid Link file : OMF File can't be multi-files (too many DSK directive).\n");
          mem_free_omfproject(current_omfproject);
          return(NULL);
        }

      /* Pas de ORG pour les OMF */
      if(current_omfproject->first_file->org_address != 0xFFFFFFFF)
        {
          printf("     => Error, Invalid Link file : Directive ORG is not allowed (Target is a Relocatable OMF File).\n");
          mem_free_omfproject(current_omfproject);
          return(NULL);
        }

      /* Project Type : OMF Mono / Multi Segment */
      if(current_omfproject->first_file->nb_segment == 1)
        current_omfproject->project_type = PROJECT_TYPE_MONO_SEG_RELOC_ADDR;
      else
        current_omfproject->project_type = PROJECT_TYPE_MULTI_SEG_RELOC_ADDR;

      /** On va générer un Program Relogeable OMF v2.1 **/
      current_omfproject->address_type      = ADDRESS_TYPE_RELATIVE_OMF;
      current_omfproject->merge_fa_segments = 0;
    }
  /** Fichier Adresse Fixe **/
  else
    {
      /* Pas de XPL pour les Fixed Address */
      if(current_omfproject->express_load == 1)
        {
          printf("     => Error, Invalid Link file : Directive XPL is not allowed (Target is a Fixed Address File).\n");
          mem_free_omfproject(current_omfproject);
          return(NULL);
        }

      /** [ORG] : On veut des Org Adresses **/
      for(current_omffile = current_omfproject->first_file; current_omffile; current_omffile=current_omffile->next)
        if(current_omffile->org_address == 0xFFFFFFFF)
          {
            printf("     => Error, Invalid Link file : Directive ORG is missing for File %s.\n",current_omffile->dsk_name);
            mem_free_omfproject(current_omfproject);
            return(NULL);
          }

      /** Mono / Multi Files Fixed Address **/
      if(current_omfproject->nb_file == 1)
        {
          /* Project Type */
          current_omfproject->project_type = PROJECT_TYPE_MONO_FILE_FIXED_ADDR;

          /** Fixed Address / Single Binary : Tous les Segments seront collés les uns derrière les autres, dans 1 fichier **/
          current_omfproject->address_type      = ADDRESS_TYPE_FIXED;
          current_omfproject->merge_fa_segments = 1;
          current_omfproject->express_load      = 0;
        }
      else
        {
          /* Project Type */
          current_omfproject->project_type = PROJECT_TYPE_MULTI_FILE_FIXED_ADDR;

          /** Fixed Address / Multi Binary : On va générer autant de fichier Binary qu'il y a de File **/
          current_omfproject->address_type      = ADDRESS_TYPE_FIXED;
          current_omfproject->merge_fa_segments = 0;
          current_omfproject->express_load      = 0;
        }
    }

  /* OK */
  return(current_omfproject);
}


/******************************************************************************************************/
/*  GetSourceFileDirective() :  Récupère les directives TYP/DSK/ORG dans les fichiers Source. */
/******************************************************************************************************/
static void GetSourceFileDirective(char *source_file_path, char *buffer_typ_rtn, char *buffer_dsk_rtn, char *buffer_org_rtn, int init)
{
  char file_name[1024];
  char put_file_path[1024];
  char *full_file_path;
  struct source_file *current_file;
  struct source_line *current_line;
  struct parameter *param;
  my_Memory(MEMORY_GET_PARAM,&param,NULL,NULL);

  /* Init */
  if(init == 1)
    {
      strcpy(buffer_typ_rtn,"");
      strcpy(buffer_dsk_rtn,"");
      strcpy(buffer_org_rtn,"");
    }

  /* Extrait le Nom du fichier du chemin */
  GetNameFromPath(source_file_path,file_name);

  /** Charge en mémoire le fichier principal Source **/
  current_file = LoadOneSourceFile(source_file_path,file_name,0);
  if(current_file == NULL)
    return;

  /** On passe toutes les lignes en revue **/
  for(current_line = current_file->first_line; current_line; current_line = current_line->next)
    {
      /* Commentaire / Vide */
      if(current_line->type == LINE_COMMENT || current_line->type == LINE_EMPTY)
        continue;

      /* Opcode TYP */
      if(!my_stricmp(current_line->opcode_txt,"TYP") && strlen(buffer_typ_rtn) == 0)
        {
          /* Récupère la valeur */
          strcpy(buffer_typ_rtn,current_line->operand_txt);

          /* Décode les Alias Ascii (S16 pour $B3) */
          DecodeDirectiveTYP(buffer_typ_rtn);
        }
      /* Opcode DSK */
      if(!my_stricmp(current_line->opcode_txt,"DSK") && strlen(buffer_dsk_rtn) == 0)
        strcpy(buffer_dsk_rtn,current_line->operand_txt);
      /* Opcode ORG */
      if(!my_stricmp(current_line->opcode_txt,"ORG") && strlen(buffer_org_rtn) == 0)
        strcpy(buffer_org_rtn,current_line->operand_txt);

      /** On recherche dans les sous fichiers sources (uniquement depuis le Master Source File) **/
      if(!my_stricmp(current_line->opcode_txt,"PUT") && init == 1)
        {
          /* Chemin du fichier */
          full_file_path = BuildProjectFilePath(current_line->operand_txt,"PUT");
          if(full_file_path != NULL)
            {
              /* Chemin complet */
              strcpy(put_file_path,full_file_path);
              free(full_file_path);

              /* Recherche dans ce fichier source (récursivité) */
              GetSourceFileDirective(put_file_path,buffer_typ_rtn,buffer_dsk_rtn,buffer_org_rtn,0);
            }
          else
            printf("  => Error : Can't build file path for '%s'\n",current_line->operand_txt);
        }
    }

  /* Libération mémoire */
  mem_free_sourcefile(current_file,0);
}

/***********************************************************************/
