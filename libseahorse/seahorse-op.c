/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <string.h>
#include <eel/eel.h>

#include "seahorse-gpgmex.h"
#include "seahorse-op.h"
#include "seahorse-util.h"
#include "seahorse-context.h"
#include "seahorse-key-source.h"
#include "seahorse-vfs-data.h"

/* Setup some basic GPGME context stuff before an operation */
static void
set_gpgme_opts (SeahorseKeySource *sksrc)
{
    gpgme_ctx_t ctx = sksrc->ctx;
    gpgme_set_armor (ctx, eel_gconf_get_boolean (ARMOR_KEY));
    gpgme_set_textmode (ctx, eel_gconf_get_boolean (TEXTMODE_KEY));
}
    
/* helper function for importing @data. @data will be released.
 * returns number of keys imported or -1. */
static gint
import_data (SeahorseKeySource *sksrc, gpgme_data_t data, gpgme_error_t *err)
{
	gint keys = 0;
    gpgme_ctx_t new_ctx;

    new_ctx = seahorse_key_source_new_context (sksrc);	
    g_return_val_if_fail (new_ctx != NULL, -1);
    
	*err = gpgme_op_import (new_ctx, data);
	if (GPG_IS_OK (*err)) {
        gpgme_import_result_t result = gpgme_op_import_result (new_ctx);
		keys = result->considered;
	}
	gpgme_data_release (data);
    gpgme_release (new_ctx);
	
	if (GPG_IS_OK (*err)) {
        seahorse_key_source_refresh (sksrc, FALSE);
		return keys;
	}
	else
		g_return_val_if_reached (-1);
}

/**
 * seahorse_op_import_file:
 * @sksrc: #SeahorseKeySource to import into
 * @path: Path of file to import
 * @err: Optional error value
 *
 * Tries to import any keys contained in the file @path, saving any error in @err.
 *
 * Returns: Number of keys imported or -1 if import fails
 **/
gint
seahorse_op_import_file (SeahorseKeySource *sksrc, const gchar *path, gpgme_error_t *err)
{
	gpgme_data_t data;
	gpgme_error_t error;
  
	if (err == NULL)
		err = &error;
	/* new data from file */
    data = seahorse_vfs_data_create (path, FALSE, err);
    g_return_val_if_fail (data != NULL, -1);
	
	return import_data (sksrc, data, err);
}

/**
 * seahorse_op_import_text:
 * @sksrc: #SeahorseKeySource to import into 
 * @text: Text to import
 * @err: Optional error value
 *
 * Tries to import any keys contained in @text, saving any errors in @err.
 *
 * Returns: Number of keys imported or -1 if import fails
 **/
gint
seahorse_op_import_text (SeahorseKeySource *sksrc, const gchar *text, gpgme_error_t *err)
{
	gpgme_data_t data;
	gpgme_error_t error;
	
	if (err == NULL)
		err = &error;

    g_return_val_if_fail (text != NULL, 0);
         
	/* new data from text */
	*err = gpgme_data_new_from_mem (&data, text, strlen (text), TRUE);
	g_return_val_if_fail (GPG_IS_OK (*err), -1);
	
	return import_data (sksrc, data, err);
}

/* helper function for exporting @keys. */
static void
export_data (GList *keys, gboolean force_armor, gpgme_data_t data,
             gpgme_error_t *err)
{
    SeahorseKeySource *sksrc;
    SeahorseKey *skey;
    gpgme_error_t error;
    gboolean old_armor;
    GList *l;
   
    if (err == NULL)
        err = &error;
       
    for (l = keys; l != NULL; l = g_list_next (l)) {
       
        g_return_if_fail (SEAHORSE_IS_KEY (l->data));
        skey = SEAHORSE_KEY (l->data);
       
        sksrc = seahorse_key_get_source (skey);
        g_return_if_fail (sksrc != NULL);

        set_gpgme_opts (sksrc);
        
        if (force_armor) {
            old_armor = gpgme_get_armor (sksrc->ctx);
            gpgme_set_armor (sksrc->ctx, TRUE);
        }
        
        *err = gpgme_op_export (sksrc->ctx, seahorse_key_get_id (skey->key), 0, data);
        
        if (force_armor)
            gpgme_set_armor (sksrc->ctx, old_armor);
        
        g_return_if_fail (GPG_IS_OK (*err));
    }
}

/**
 * seahorse_op_export_file:
 * @keys: List of #SeahorseKey objects to export
 * @path: Path of a new file to export to
 * @err: Optional error value
 *
 * Tries to export @recips to the new file @path, saving an errors in @err.
 * @recips will be released upon completion.
 **/
void
seahorse_op_export_file (GList *keys, const gchar *path, gpgme_error_t *err)
{
	gpgme_data_t data;
	gpgme_error_t error;
	
	if (err == NULL)
		err = &error;

    /* Open the appropriate file */
    data = seahorse_vfs_data_create (path, TRUE, err);
    g_return_if_fail (GPG_IS_OK (*err));
            
	/* export data */
	export_data (keys, FALSE, data, err);
	g_return_if_fail (GPG_IS_OK (*err));
  
    /* Close the file */
    gpgme_data_release (data);
}

/**
 * seahorse_op_export_text:
 * @keys: List of #SeahorseKey objects to export
 * @err: Optional error value
 *
 * Tries to export @recips to text using seahorse_util_write_data_to_text(),
 * saving any errors in @err. @recips will be released upon completion.
 * ASCII Armor setting of @sctx will be ignored.
 *
 * Returns: The exported text or NULL if the operation fails
 **/
gchar*
seahorse_op_export_text (GList *keys, gpgme_error_t *err)
{
	gpgme_data_t data;
	gpgme_error_t error;
	
	if (err == NULL)
		err = &error;
        
    *err = gpgme_data_new (&data);
    g_return_val_if_fail (GPG_IS_OK (*err), NULL);
    
	/* export data with armor */
	export_data (keys, TRUE, data, err);
	g_return_val_if_fail (GPG_IS_OK (*err), NULL);
	
	return seahorse_util_write_data_to_text (data);
}

/* common encryption function definition. */
typedef gpgme_error_t (* EncryptFunc) (gpgme_ctx_t ctx, gpgme_key_t recips[],
				       gpgme_encrypt_flags_t flags,
				       gpgme_data_t plain, gpgme_data_t cipher);

/* helper function for encrypting @plain to @recips using @func. @plain and
 * @keys will be released.*/
static void
encrypt_data_common (SeahorseKeySource *sksrc, GList *keys, gpgme_data_t plain, 
                     gpgme_data_t cipher, EncryptFunc func, gboolean force_armor, 
                     gpgme_error_t *err)
{
    SeahorseKey *skey;
    gpgme_key_t *recips;
    gboolean old_armor;
    gchar *id;

	/* if don't already have an error, do encrypt */
	if (GPG_IS_OK (*err)) {

        /* Add the default key if set and necessary */
        if (eel_gconf_get_boolean (ENCRYPTSELF_KEY)) {
            id = eel_gconf_get_string (DEFAULT_KEY);        
            if (id != NULL) {
                skey = seahorse_key_source_get_key (sksrc, id, TRUE);
                if (skey != NULL)
                    keys = g_list_append (keys, skey);
            }
        }

        /* Make keys into the right format */
        recips = seahorse_util_list_to_keys (keys);
        
        set_gpgme_opts (sksrc);

        if (force_armor) {
            old_armor = gpgme_get_armor (sksrc->ctx);
            gpgme_set_armor (sksrc->ctx, TRUE);
        }
              
		*err = func (sksrc->ctx, recips, GPGME_ENCRYPT_ALWAYS_TRUST, plain, cipher);
        
        if (force_armor)
            gpgme_set_armor (sksrc->ctx, old_armor);
         
        seahorse_util_free_keys (recips);
	}
	/* release plain  */
	gpgme_data_release (plain);
}

/* common file encryption helper to encrypt @path to @recips using @func.
 * @keys will be released.*/
static void
encrypt_file_common (GList *keys, const gchar *path, const gchar *epath,
                     EncryptFunc func, gpgme_error_t *err)
{
    SeahorseKeySource *sksrc;
	gpgme_data_t plain, cipher;
	gpgme_error_t error;
	
	if (err == NULL)
		err = &error;

    /* When S/MIME supported need to make sure all keys from the same source */       
    g_return_if_fail (keys && SEAHORSE_IS_KEY (keys->data));
    sksrc = seahorse_key_get_source (SEAHORSE_KEY (keys->data));
    g_return_if_fail (sksrc != NULL);

    plain = seahorse_vfs_data_create (path, FALSE, err);
    g_return_if_fail (plain != NULL);
    
    cipher = seahorse_vfs_data_create (epath, TRUE, err);
    if (!cipher) {
        gpgme_data_release (plain);
        g_return_if_reached ();
    }
 
	encrypt_data_common (sksrc, keys, plain, cipher, func, FALSE, err);
	g_return_if_fail (GPG_IS_OK (*err));

    gpgme_data_release (cipher);	
}

/* common text encryption helper to encrypt @text to @recips using @func.
 * ASCII Armor setting of @sctx is ignored. returns the encrypted text. */
static gchar*
encrypt_text_common (GList *keys, const gchar *text, EncryptFunc func, 
                     gpgme_error_t *err)
{
    SeahorseKeySource *sksrc;
	gpgme_data_t plain, cipher;
	gpgme_error_t error;
	
	if (err == NULL)
		err = &error;

    /* When S/MIME supported need to make sure all keys from the same source */       
    g_return_val_if_fail (keys && SEAHORSE_IS_KEY (keys->data), NULL);
    sksrc = seahorse_key_get_source (SEAHORSE_KEY (keys->data));
    g_return_val_if_fail (sksrc != NULL, NULL);
    
	/* new data form text */
	*err = gpgme_data_new_from_mem (&plain, text, strlen (text), TRUE);
    g_return_val_if_fail (GPG_IS_OK (*err), NULL);
    
    *err = gpgme_data_new (&cipher);
    g_return_val_if_fail (GPG_IS_OK (*err), NULL);
   
	/* encrypt with armor */
	encrypt_data_common (sksrc, keys, plain, cipher, func, TRUE, err);
	g_return_val_if_fail (GPG_IS_OK (*err), NULL);
	
	return seahorse_util_write_data_to_text (cipher);
}

/**
 * seahorse_op_encrypt_file:
 * @keys: List of #SeahorseKey objects to encrypt to
 * @path: Path of file to encrypt
 * @epath: Path of file to write encrypted data
 * @err: Optional error value
 *
 * Tries to encrypt the file @path to @recips, saving any errors in @err.
 **/
void
seahorse_op_encrypt_file (GList *keys, const gchar *path, const gchar *epath,
                          gpgme_error_t *err)
{
	return encrypt_file_common (keys, path, epath, gpgme_op_encrypt, err);
}

/**
 * seahorse_op_encrypt_text:
 * @keys: List of #SeahorseKey objects to encrypt to
 * @text: Text to encrypt
 * @err: Optional error value
 *
 * Tries to encrypt @text to @recips, saving any errors in @err. The ASCII Armor
 * setting of @sctx will be ignored. @recips will be released upon completion.
 *
 * Returns: The encrypted text or NULL if encryption fails
 **/
gchar*
seahorse_op_encrypt_text (GList *keys, const gchar *text, gpgme_error_t *err)
{
	return encrypt_text_common (keys, text, gpgme_op_encrypt, err);
}

/* Helper function to set a key pair as the signer for its keysource */
static void
set_signer (SeahorseKeyPair *signer)
{
    SeahorseKeySource *sksrc;
    
    sksrc = seahorse_key_get_source (SEAHORSE_KEY (signer));
    g_return_if_fail (sksrc != NULL);
    
    gpgme_signers_clear (sksrc->ctx);
    gpgme_signers_add (sksrc->ctx, signer->secret);
}

/* helper function for signing @plain with @mode. @plain will be released. */
static void
sign_data (SeahorseKeySource *sksrc, gpgme_data_t plain, gpgme_data_t sig,
           gpgme_sig_mode_t mode, gpgme_error_t *err)
{
    set_gpgme_opts (sksrc);

	*err = gpgme_op_sign (sksrc->ctx, plain, sig, mode);
    	
	gpgme_data_release (plain);
}

/**
 * seahorse_op_sign_file:
 * @signer: Key pair to sign with 
 * @path: Path of file to sign
 * @spath: Where to write the signature
 * @err: Optional error value
 *
 * Tries to create a detached signature file for the file @path, saving any errors
 * in @err. 
 **/
void
seahorse_op_sign_file (SeahorseKeyPair *signer, const gchar *path, 
                       const gchar *spath, gpgme_error_t *err)
{
    SeahorseKeySource *sksrc;
	gpgme_data_t plain, sig;
	gpgme_error_t error;
	
	if (err == NULL)
		err = &error;

    sksrc = seahorse_key_get_source (SEAHORSE_KEY (signer));
    g_return_if_fail (sksrc != NULL);
    
	/* new data from file */
    plain = seahorse_vfs_data_create (path, FALSE, err);
    g_return_if_fail (plain != NULL);
    
    sig = seahorse_vfs_data_create (spath, TRUE, err);
    if (!sig) {
        gpgme_data_release (plain);
        g_return_if_reached ();
    }
  
    set_signer (signer);
    
	/* get detached signature */
	sign_data (sksrc, plain, sig, GPGME_SIG_MODE_DETACH, err);
	g_return_if_fail (GPG_IS_OK (*err));
  
    gpgme_data_release (sig);
}

/**
 * seahorse_op_sign_text:
 * @sksrc: SeahorseKeyStore to sign with 
 * @text: Text to sign
 * @err: Optional error value
 *
 * Tries to sign @text using a clear text signature, saving any errors in @err.
 * Signing will be done by the default key of @sctx.
 *
 * Returns: The clear signed text or NULL if signing fails
 **/
gchar*
seahorse_op_sign_text (SeahorseKeyPair *signer, const gchar *text, 
                       gpgme_error_t *err)
{
    SeahorseKeySource *sksrc;
	gpgme_data_t plain, sig;
	gpgme_error_t error;
	
	if (err == NULL)
		err = &error;

    sksrc = seahorse_key_get_source (SEAHORSE_KEY (signer));
    g_return_val_if_fail (sksrc != NULL, NULL);
    
    set_signer (signer);
            
	/* new data from text */
	*err = gpgme_data_new_from_mem (&plain, text, strlen (text), TRUE);
	g_return_val_if_fail (GPG_IS_OK (*err), NULL);
    
    *err = gpgme_data_new (&sig);
    g_return_val_if_fail (GPG_IS_OK (*err), NULL);    
    
	/* clear sign data already ignores ASCII Armor */
	sign_data (sksrc, plain, sig, GPGME_SIG_MODE_CLEAR, err);
	g_return_val_if_fail (GPG_IS_OK (*err), NULL);
	
	return seahorse_util_write_data_to_text (sig);
}

/**
 * seahorse_op_encrypt_sign_file:
 * @keys: List of #SeahorseKey objects to encrypt to
 * @path: Path of file to encrypt and sign
 * @epath: Where to write encrypted data
 * @recips: Keys to encrypt with
 * @err: Optional error value
 *
 * Tries to encrypt and sign the file @path to @recips, saving any errors in @err.
 * Signing will be done with the default key of @sctx. @recips will be released
 * upon completion.
 **/
void
seahorse_op_encrypt_sign_file (GList *keys, SeahorseKeyPair *signer, 
                               const gchar *path, const gchar *epath, gpgme_error_t *err)
{
    set_signer (signer);
	encrypt_file_common (keys, path, epath, gpgme_op_encrypt_sign, err);
}

/**
 * seahorse_op_encrypt_sign_text:
 * @keys: List of #SeahorseKey objects to encrypt to
 * @text: Text to encrypt and sign
 * @recips: Keys to encrypt with
 * @err: Optional error value
 *
 * Tries to encrypt and sign @text to @recips, saving any errors in @err.
 * Signing will be done with the default key of @sctx. @recips will be released
 * upon completion. The ASCII Armor setting of @sctx will be ignored.
 *
 * Returns: The encrypted and signed text or NULL if the operation fails
 **/
gchar*
seahorse_op_encrypt_sign_text (GList *keys, SeahorseKeyPair *signer, 
                               const gchar *text, gpgme_error_t *err)
{
    set_signer (signer);
	return encrypt_text_common (keys, text, gpgme_op_encrypt_sign, err);
}

/**
 * seahorse_op_verify_file:
 * @sksrc: #SeahorseKeySource to verify against
 * @path: Path of detached signature file
 * @status: Will contain the status of any verified signatures
 * @err: Optional error value
 *
 * Tries to verify the signature file @path, saving any errors in @err. The
 * signed file to check against is assumed to be @path without the suffix.
 * The status of any verified signatures will be saved in @status.
 **/
void
seahorse_op_verify_file (SeahorseKeySource *sksrc, const gchar *path, const gchar *original,
                         gpgme_verify_result_t *status, gpgme_error_t *err)
{
	gpgme_data_t sig, plain;
	gpgme_error_t error;
	
	if (err == NULL)
		err = &error;
	/* new data from sig file */
    sig = seahorse_vfs_data_create (path, FALSE, err);
    g_return_if_fail (plain != NULL);

    plain = seahorse_vfs_data_create (original, TRUE, err);
	if (!plain) {
		gpgme_data_release (sig);
		g_return_if_reached ();
	}
 
	/* verify sig file, release plain data */
    *err = gpgme_op_verify (sksrc->ctx, sig, plain, NULL);
    *status = gpgme_op_verify_result (sksrc->ctx);
    gpgme_data_release (sig); 
	gpgme_data_release (plain);
	g_return_if_fail (GPG_IS_OK (*err));
}

/**
 * seahorse_op_verify_text:
 * @sksrc: #SeahorseKeySource to verify against
 * @text: Text to verify
 * @status: Will contain the status of any verified signatures
 * @err: Optional error value
 *
 * Tries to verify @text, saving any errors in @err. The status of any verified
 * signatures will be saved in @status. The ASCII Armor setting of @sctx will
 * be ignored.
 *
 * Returns: The verified text or NULL if the operation fails
 **/
gchar*
seahorse_op_verify_text (SeahorseKeySource *sksrc, const gchar *text,
                         gpgme_verify_result_t *status, gpgme_error_t *err)
{
	gpgme_data_t sig, plain;
	gpgme_error_t error;
	gboolean armor;
	
	if (err == NULL)
		err = &error;
	/* new data from text */
	*err = gpgme_data_new_from_mem (&sig, text, strlen (text), TRUE);
	g_return_val_if_fail (GPG_IS_OK (*err), NULL);
	/* new data to save verified text */
	*err = gpgme_data_new (&plain);
	/* free text data if error */
	if (!GPG_IS_OK (*err)) {
		gpgme_data_release (sig);
		g_return_val_if_reached (NULL);
	}
	/* verify data with armor */
	armor = gpgme_get_armor (sksrc->ctx);
	gpgme_set_armor (sksrc->ctx, TRUE);
    /* verify sig file, release plain data */
    *err = gpgme_op_verify (sksrc->ctx, sig, NULL, plain);
    *status = gpgme_op_verify_result (sksrc->ctx);
    gpgme_data_release (sig);     
	gpgme_set_armor (sksrc->ctx, armor);
	g_return_val_if_fail (GPG_IS_OK (*err), NULL);
	/* return verified text */
	return seahorse_util_write_data_to_text (plain);
}

/* helper function to decrypt and verify @cipher. @cipher will be released. */
static void
decrypt_verify_data (SeahorseKeySource *sksrc, gpgme_data_t cipher,
                     gpgme_data_t plain, gpgme_verify_result_t *status, 
                     gpgme_error_t *err)
{
    set_gpgme_opts (sksrc);
	
	*err = gpgme_op_decrypt_verify (sksrc->ctx, cipher, plain);
        
    if (status)
    	*status = gpgme_op_verify_result (sksrc->ctx);
     
	gpgme_data_release (cipher);
}

/**
 * seahorse_op_decrypt_verify_file:
 * @sksrc: #SeahorseKeySource to verify against
 * @path: Path of file to decrypt and verify
 * @path: Where to write the decrypted data
 * @status: Will contain the status of any verified signatures
 * @err: Optional error value
 *
 * Tries to decrypt and verify the file @path, saving any errors in @err. The
 * status of any verified signatures will be saved in @status. 
 **/
void
seahorse_op_decrypt_verify_file (SeahorseKeySource *sksrc, const gchar *path, 
                                 const gchar *dpath, gpgme_verify_result_t *status, 
                                 gpgme_error_t *err)
{
	gpgme_data_t cipher, plain;
	gpgme_error_t error;
	
	if (err == NULL)
		err = &error;
	/* new data from file */
    cipher = seahorse_vfs_data_create (path, FALSE, err);
    g_return_if_fail (cipher != NULL);
    
    plain = seahorse_vfs_data_create (dpath, TRUE, err);
    if (!plain) {
        gpgme_data_release (cipher);
        g_return_if_reached ();
    }

	/* verify data */
	decrypt_verify_data (sksrc, cipher, plain, status, err);

    gpgme_data_release (plain);
}

/**
 * seahorse_op_decrypt_verify_text:
 * @sksrc: #SeahorseKeySource to verify against
 * @text: Text to decrypt and verify
 * @status: Will contain the status of any verified signatures
 * @err: Optional error value
 *
 * Tries to decrypt and verify @text, saving any errors in @err. The status of
 * any verified signatures will be saved in @status. The ASCII Armor setting
 * of @sctx will be ignored.
 *
 * Returns: The decrypted text or NULL if the operation fails
 **/
gchar*
seahorse_op_decrypt_verify_text (SeahorseKeySource *sksrc, const gchar *text,
                                 gpgme_verify_result_t *status, gpgme_error_t *err)
{
	gpgme_data_t cipher, plain;
	gpgme_error_t error;
	gboolean armor;
	
	if (err == NULL)
		err = &error;
	/* new data from text */
	*err = gpgme_data_new_from_mem (&cipher, text, strlen (text), TRUE);
	g_return_val_if_fail (GPG_IS_OK (*err), NULL);
    
    *err = gpgme_data_new (&plain);
    g_return_val_if_fail (GPG_IS_OK (*err), NULL);
    
	/* get decrypted data with armor */
	armor = gpgme_get_armor (sksrc->ctx);
	gpgme_set_armor (sksrc->ctx, TRUE);
	decrypt_verify_data (sksrc, cipher, plain, status, err);
	gpgme_set_armor (sksrc->ctx, armor);
	g_return_val_if_fail (GPG_IS_OK (*err), NULL);
	/* return text of decrypted data */
	return seahorse_util_write_data_to_text (plain);
}
