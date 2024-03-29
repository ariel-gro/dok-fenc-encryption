/*!	\file libfenc_WatersCP.c
 *
 *	\brief Routines for the Waters CP-ABE scheme.
 *  
 *	Copyright 2009 Matthew Green. All rights reserved.
 */

#include <stdlib.h>
#include <string.h>
#include <pbc/pbc.h>
#include "libfenc.h"
#include "libfenc_group_params.h"
#include "libfenc_ABE_common.h"
#include "libfenc_utils.h"
#include "libfenc_WatersCP.h"
#include "libfenc_LSSS.h"
#include "libfenc_LSW.h"
#include <time.h>
/********************************************************************************
 * Waters Ciphertext-Policy Implementation
 ********************************************************************************/

/*!
 * Initialize a fenc_context data structure for use with the Waters scheme.  
 * Any number of fenc_context structures may be simultaneously used, with the same
 * or different schemes.  The caller assumes responsible for allocating the context
 * buffer.
 *
 * @param context		Pre-allocated buffer for the fenc_context data structure.
 * @return				FENC_ERROR_NONE or an error code.
 */

FENC_ERROR
libfenc_create_context_WatersCP(fenc_context *context)
{
	CHECK_LIBRARY_STATE;
	
	FENC_ERROR result = FENC_ERROR_UNKNOWN;
	
	/* Allocate a scheme-specific context. */
	context->scheme_context = SAFE_MALLOC( sizeof(fenc_scheme_context_WatersCP) );
	
	if (context->scheme_context != NULL) {
		/* Set up the scheme context. */
		memset(context->scheme_context, 0, sizeof(fenc_scheme_context_WatersCP) );
		
		/* TODO */
		result = FENC_ERROR_NONE;
	} else {
		 /* Couldn't allocate scheme context. */
		 result = FENC_ERROR_OUT_OF_MEMORY;
	}
	
	/* Configure  function pointers within the fenc_context to point to
	 * LSW scheme-specific routines.									*/
	if (result == FENC_ERROR_NONE) {
		context->gen_params				= libfenc_gen_params_WatersCP;
		context->set_params				= libfenc_set_params_WatersCP;
		context->extract_key			= libfenc_extract_key_WatersCP;
		context->encrypt				= libfenc_encrypt_WatersCP;
		context->kem_encrypt			= libfenc_kem_encrypt_WatersCP;
		context->decrypt				= libfenc_decrypt_WatersCP;
		context->destroy_context		= libfenc_destroy_context_WatersCP;
		context->generate_global_params	= libfenc_generate_global_params_COMMON;
		context->destroy_global_params	= libfenc_destroy_global_params_COMMON;
		context->export_public_params	= libfenc_export_public_params_WatersCP;
		context->export_secret_params	= libfenc_export_secret_params_WatersCP;
		context->import_public_params	= libfenc_import_public_params_WatersCP;
		context->import_secret_params	= libfenc_import_secret_params_WatersCP;
		context->export_global_params	= libfenc_export_global_params_WatersCP;
		context->import_global_params	= libfenc_import_global_params_WatersCP;
		context->export_secret_key		= libfenc_export_secret_key_WatersCP;
		context->import_secret_key		= libfenc_import_secret_key_WatersCP;
	}
		
	/* Return success/error. */
	return result;
}

/*!
 * Generate public and secret parameters.
 *
 * @param context		The fenc_context data structure
 * @param global_params	Global params (scheme-specific).
 * @return				FENC_ERROR_NONE or an error code.
 */

FENC_ERROR
libfenc_gen_params_WatersCP(fenc_context *context, fenc_global_params *global_params)
{
	FENC_ERROR result = FENC_ERROR_UNKNOWN, err_code = FENC_ERROR_NONE;
	element_t eggT, aZ;
	fenc_scheme_context_WatersCP* scheme_context;
	Bool elements_initialized = FALSE;
	
	/* Get the scheme-specific context. */
	scheme_context = (fenc_scheme_context_WatersCP*)context->scheme_context;
	if (scheme_context == NULL) {
		result = FENC_ERROR_INVALID_CONTEXT;
		goto cleanup;
	}
	
	/* Validate the global parameters. */
	err_code = libfenc_validate_global_params_WatersCP(global_params);
	if (err_code != FENC_ERROR_NONE) {
		LOG_ERROR("%s: could not validate global params, error: %s", __func__, libfenc_error_to_string(err_code));
		result = err_code;
		goto cleanup;
	}
	
	/* Global parameters check out ok.  Copy them and generate the scheme-specific parameters.  The NULL
	 * parameter causes the structure to be allocated.		*/
	scheme_context->global_params = initialize_global_params_WatersCP(global_params->group_params, NULL);
	
	/* Initialize the elements in the public and secret parameters, along with some temporary variables. */
	public_params_initialize_WatersCP(&(scheme_context->public_params), scheme_context->global_params->pairing);
	secret_params_initialize_WatersCP(&(scheme_context->secret_params), scheme_context->global_params->pairing);
	element_init_GT(eggT, scheme_context->global_params->pairing);
	element_init_Zr(aZ, scheme_context->global_params->pairing);
	elements_initialized = TRUE;
	
	/* Select randoms generators g1 \in G1, g2 \in G2, a, \alpha */
	element_random(scheme_context->public_params.gONE);
	element_random(scheme_context->public_params.gTWO);
	element_random(scheme_context->secret_params.alphaZ);
	element_random(aZ);												
	element_random(scheme_context->secret_params.alphaZ);								
	
	/* Compute g^a, */
	element_pow_zn(scheme_context->public_params.gaONE, scheme_context->public_params.gONE, aZ);	/* gaONE = gONE^a */
	element_pow_zn(scheme_context->public_params.gaTWO, scheme_context->public_params.gTWO, aZ);    /* gaTWO = gaTWO^a */
	
	/* Compute eggalphaT = e(gONE,gTWO)^(alpha) */
	pairing_apply(eggT, scheme_context->public_params.gONE, scheme_context->public_params.gTWO, scheme_context->global_params->pairing);	/* eggT = e(gONE, gTWO) */
	element_pow_zn(scheme_context->public_params.eggalphaT, eggT, scheme_context->secret_params.alphaZ);									/* eggalphaT = eggT^alpha */

	/* Success */
	result = FENC_ERROR_NONE;
	
cleanup:
	if (elements_initialized == TRUE) {
		/* Destroy any temporary elements. */
		element_clear(eggT);
		element_clear(aZ);
	}
	
	return result;
}

/*!
 * Load public and (optionally) secret parameters into the context.  All relevant global
 * parameters are embedded within the public_params data structure.
 *
 * @param context		The fenc_context data structure
 * @param public_params	Public scheme parameters.
 * @param secret_params	Secret scheme parameters (optional).
 * @return				FENC_ERROR_NONE or an error code.
 */

FENC_ERROR
libfenc_set_params_WatersCP(fenc_context *context, fenc_public_params *public_params, fenc_secret_params *secret_params)
{
	return FENC_ERROR_NOT_IMPLEMENTED;
}

/*!
 * Extract a secret key representing a given function input, which is defined as an access structure.
 * Note that this function will only be called if the secret parameters (MSK) are available within 
 * the context.
 *
 * @param context		The fenc_context data structure
 * @param input			The function input from which this key will be built.
 * @param key			A pre-allocated buffer for the resulting key
 * @return				FENC_ERROR_NONE or an error code.
 */

FENC_ERROR
libfenc_extract_key_WatersCP(fenc_context *context, fenc_function_input *input, fenc_key *key)
{
	FENC_ERROR					result = FENC_ERROR_UNKNOWN, err_code = FENC_ERROR_NONE;
	fenc_key_WatersCP			*key_WatersCP = NULL;
	fenc_attribute_list			*attribute_list = NULL;
	fenc_scheme_context_WatersCP*	scheme_context;
	int							i;
	element_t					tZ, hashONE, tempONE, temp2ONE, tempZ, temp2Z, tempTWO, temp2TWO;
	Bool						elements_initialized = FALSE;
	
	/* Get the scheme-specific context. */
	scheme_context = (fenc_scheme_context_WatersCP*)context->scheme_context;
	if (scheme_context == NULL) {
		result = FENC_ERROR_INVALID_CONTEXT;
		goto cleanup;
	}
	
	/* Parse the function input as an attribute list.  This will allocate memory
	 * that will ultimately be released when the key is cleared.				*/
	attribute_list = SAFE_MALLOC(sizeof(fenc_attribute_list));
	err_code = libfenc_parse_input_as_attribute_list(input, attribute_list, scheme_context->global_params->pairing);
	if (err_code != FENC_ERROR_NONE) {
		LOG_ERROR("%s: could not parse function input as an attribute list", __func__);
		result = FENC_ERROR_INVALID_INPUT;
		goto cleanup;
	}
		
	/* Initialize the LSW-specific key data structure and allocate some temporary variables.	*/
	key_WatersCP = fenc_key_WatersCP_initialize(attribute_list, FALSE, scheme_context->global_params);
	if (key_WatersCP == NULL) {
		LOG_ERROR("%s: could not initialize key structure", __func__);
		result = FENC_ERROR_INVALID_INPUT;
		goto cleanup;
	}
	element_init_Zr(tZ, scheme_context->global_params->pairing);
	element_init_G1(tempONE, scheme_context->global_params->pairing);
	element_init_G2(tempTWO, scheme_context->global_params->pairing);
	element_init_G2(temp2TWO, scheme_context->global_params->pairing);
	elements_initialized = TRUE;
	
	/* Select a random tZ.	*/
	element_random(tZ);
	
	/* Compute KTWO = gTWO^{\alpha} * gTWO^{at}.	*/
	element_pow_zn(tempTWO, scheme_context->public_params.gaTWO, tZ);									/* tempTWO = gTWO^{at}			*/
	element_pow_zn(temp2TWO, scheme_context->public_params.gTWO, scheme_context->secret_params.alphaZ);	/* temp2TWO = gTWO^\alpha		*/
	element_mul(key_WatersCP->KTWO, tempTWO, temp2TWO);													/* KONE = tempTWO * temp2TWO	*/
	
	/* Compute LTWO = gTWO^t.		*/
	element_pow_zn(key_WatersCP->LTWO, scheme_context->public_params.gTWO, tZ);							/* LTWO = gTWO^{t}				*/
	
	/* For every share/attribute, create one component of the secret key.	*/
	for (i = 0; i < (signed int)key_WatersCP->attribute_list.num_attributes; i++) {		
		/* Hash the attribute string to Zr, if it hasn't already been.	*/
		hash_attribute_string_to_Zr(&(key_WatersCP->attribute_list.attribute[i]), scheme_context->global_params->pairing);
		
		/* Now hash the result into an element of G1 (tempONE).	*/
		err_code = hash2_attribute_element_to_G1(&(key_WatersCP->attribute_list.attribute[i].attribute_hash), &tempONE);	/* result in tempONE  */
		
		/* Compute KXONE[i] = tempONE^{t}.						*/
		element_pow_zn(key_WatersCP->KXONE[i], tempONE, tZ);											/* KXONE[i] = tempONE^{t}			*/
	}
	
	/* Stash the key_WatersCP structure inside of the fenc_key.		*/
	memset(key, 0, sizeof(fenc_key));
	key->scheme_type = FENC_SCHEME_WATERSCP;
	key->valid = TRUE;
	key->scheme_key = (void*)key_WatersCP;
	
	/* Success!		*/
	result = FENC_ERROR_NONE;
	
cleanup:
	/* If there was an error, clean up after ourselves.	*/
	if (result != FENC_ERROR_NONE) {
		if (key_WatersCP != NULL) {		
			fenc_attribute_list_clear(&(key_WatersCP->attribute_list));

			/* Clear out the key internals.		*/
			if (elements_initialized == TRUE)	{
				fenc_key_WatersCP_clear(key_WatersCP);
			}
		}
	}
	
	/* Wipe out temporary variables.	*/
	if (elements_initialized == TRUE) {
		element_clear(tZ);
		element_clear(tempONE);
		element_clear(tempTWO);
		element_clear(temp2TWO);
	}
	
	return result;
}

/*!
 * Encrypt a plaintext, return a ciphertext.
 *
 * @param context		The fenc_context data structure
 * @param input			The function input under which which the ciphertext will be encrypted.
 * @param plaintext		The plaintext message.
 * @param ciphertext	A pre-allocated buffer for the returned fenc_ciphertext.
 * @return				FENC_ERROR_NONE or an error code.
 */

FENC_ERROR
libfenc_encrypt_WatersCP(fenc_context *context, fenc_function_input *input, fenc_plaintext *plaintext,
					fenc_ciphertext *ciphertext)
{
	return encrypt_WatersCP_internal(context, input, plaintext, FALSE, NULL, 0, ciphertext);
}

/*!
 * Key encapsulation variant of encryption.  Generate an encryption key and encapsulate it under 
 * a given function input.  Returns the encapsulated key as well as the ciphertext.
 *
 * @param context		The fenc_context data structure
 * @param input			The function input under which which the ciphertext will be encrypted.
 * @param key_len		Desired key size (in bytes).  Will be overwritten with the actual key size.
 * @param key			Pointer to an initialized buffer into which the key will be written.
 * @param ciphertext	A pre-allocated buffer for the returned fenc_ciphertext.
 * @return				FENC_ERROR_NONE or an error code.
 */

FENC_ERROR
libfenc_kem_encrypt_WatersCP(fenc_context *context, fenc_function_input *input, size_t key_len,
									uint8* key, fenc_ciphertext *ciphertext)
{
	return encrypt_WatersCP_internal(context, input, NULL, TRUE, key, key_len, ciphertext);
}

/*!
 * Decrypt a ciphertext using a specified secret key.
 *
 * @param context		The fenc_context data structure
 * @param ciphertext	The ciphertext to decrypt.
 * @param key			The secret key to use.
 * @param plaintext		A pre-allocated buffer for the resulting plaintext.
 * @return				FENC_ERROR_NONE or an error code.
 */

FENC_ERROR
libfenc_decrypt_WatersCP(fenc_context *context, fenc_ciphertext *ciphertext, fenc_key *key,
										 fenc_plaintext *plaintext)
{
	FENC_ERROR						result = FENC_ERROR_UNKNOWN, err_code;
	fenc_ciphertext_WatersCP		ciphertext_WatersCP;
	fenc_scheme_context_WatersCP	*scheme_context = NULL;
	fenc_key_WatersCP				*key_WatersCP = NULL;
	fenc_attribute_list				attribute_list;
	fenc_attribute_policy			policy;
	fenc_lsss_coefficient_list		coefficient_list;
	element_t						tempGT, temp2GT, tempONE, temp2ONE, temp3ONE, tempZ, temp2Z, prodONE;
	element_t						temp3GT, temp4GT, prodT, finalT;
	uint32							i, j;
	int32							index_ciph, index_key;
	Bool							elements_initialized = FALSE, coefficients_initialized = FALSE;
	Bool							attribute_list_N_initialized = FALSE;
	// char							test_str[MAX_POLICY_STR];

	/* Get the scheme-specific context. */
	scheme_context = (fenc_scheme_context_WatersCP*)context->scheme_context;
	if (scheme_context == NULL) {
		return FENC_ERROR_INVALID_CONTEXT;
	}
	/* Obtain the WatersCP-specific key data structure and make sure it's correct.	*/
	if (key->scheme_key == NULL) {
		LOG_ERROR("%s: could not obtain scheme-specific decryption key", __func__);
		return FENC_ERROR_INVALID_KEY;
	}
	key_WatersCP = (fenc_key_WatersCP*)key->scheme_key;
	err_code = attribute_list_compute_hashes(&(key_WatersCP->attribute_list), scheme_context->global_params->pairing);

	/* Deserialize the ciphertext.	*/
	err_code = libfenc_deserialize_ciphertext_WatersCP(ciphertext->data, ciphertext->data_len, &ciphertext_WatersCP, scheme_context);
	if (err_code != FENC_ERROR_NONE) {
		LOG_ERROR("%s: unable to deserialize ciphertext", __func__);
		result = err_code;
		goto cleanup;
	}

#ifdef FENC_DEBUG	
//	libfenc_fprint_ciphertext_WatersCP(&ciphertext_WatersCP, stdout);
#endif			
	/* Now deserialize the policy string into a data structure and make sure all attributes are hashed.	*/
	fenc_policy_from_string(&policy, ciphertext_WatersCP.policy_str);
#ifdef FENC_DEBUG
//	strcpy(test_str, "");
//	fenc_attribute_policy_to_string(policy.root, test_str, MAX_POLICY_STR);
//	printf("PARSED ATTRIBUTE STRING: %s\n", test_str);
#endif
	err_code = attribute_tree_compute_hashes(policy.root, scheme_context->global_params->pairing);

	//libfenc_fprint_ciphertext_WatersCP(&ciphertext_WatersCP, stdout);
	
	/* Use the LSSS-associated procedure to recover the coefficients.  This gives us a list of coefficients
	 * that should match up in a 1-to-1 fashion with the components of the decryption key. 
	 * First, initialize a coefficients list:													*/
	err_code = LSSS_allocate_coefficient_list(&coefficient_list, ciphertext_WatersCP.attribute_list.num_attributes, scheme_context->global_params->pairing);
	if (err_code != FENC_ERROR_NONE) {
		LOG_ERROR("%s: could not allocate coefficients", __func__);
		result = err_code;
		goto cleanup;
	}
	coefficients_initialized = TRUE;

	/* Now calculate the actual coefficients.													*/
	err_code = fenc_LSSS_calculate_coefficients_from_policy(&policy, &(key_WatersCP->attribute_list), &coefficient_list,
															scheme_context->global_params->pairing);
	if (err_code != FENC_ERROR_NONE) {
		LOG_ERROR("%s: unable to compute LSSS coefficients", __func__);
		result = FENC_ERROR_INVALID_CIPHERTEXT;
		goto cleanup;
	}
	/* Allocate some temporary work variables.	*/
	elements_initialized = TRUE;
	element_init_GT(tempGT, scheme_context->global_params->pairing);
	element_init_GT(temp2GT, scheme_context->global_params->pairing);
	element_init_GT(temp3GT, scheme_context->global_params->pairing);
	element_init_GT(temp4GT, scheme_context->global_params->pairing);
	element_init_GT(prodT, scheme_context->global_params->pairing);
	element_init_GT(finalT, scheme_context->global_params->pairing);
	element_init_G1(prodONE, scheme_context->global_params->pairing);
	element_init_G1(tempONE, scheme_context->global_params->pairing);
	element_init_G1(temp2ONE, scheme_context->global_params->pairing);
	element_init_G1(temp3ONE, scheme_context->global_params->pairing);
	element_init_Zr(tempZ, scheme_context->global_params->pairing);
	element_init_Zr(temp2Z, scheme_context->global_params->pairing);

	/* Now compute the product of the sub-components raised to the coefficients:
	 *		prodT = \prod_{i=0}^num_attributes (Z_i^{coefficient[i]}).  
	 * We compute one of these for every value in attribute_list_N.	 */
	element_set1(prodT);
	element_set1(prodONE);
	for (i = 0; i < coefficient_list.num_coefficients; i++) {		

		if (coefficient_list.coefficients[i].is_set == TRUE) {
			/* 
			 * Let index_ciph be the attribute's index in the ciphertext structure.
			 * Let index_key be the attribute's index in the key structure.
			 *
			 * Compute finalT = e(CONE[index_ciph] , LTWO) * e(DTWO[index_ciph] , KXONE[index_key]).		*/

			/* Find the corresponding indices in the ciphertext attribute list and in the key.	*/
			DEBUG_ELEMENT_PRINTF("running libfenc_get_attribute_index_in_list\n");
			index_key = libfenc_get_attribute_index_in_list(&(ciphertext_WatersCP.attribute_list.attribute[i]), &(key_WatersCP->attribute_list));
			index_ciph = i;/*libfenc_get_attribute_index_in_list(&(attribute_list.attribute[i]), &(key_WatersCP.attribute_list));*/
				
			if (index_ciph < 0 || index_key < 0) {
				DEBUG_ELEMENT_PRINTF("ciphertext element %d=%B\n", index_ciph, ciphertext_WatersCP.attribute_list.attribute[i].attribute_hash);
				DEBUG_ELEMENT_PRINTF("ciphertext element %d=%s\n", index_ciph, ciphertext_WatersCP.attribute_list.attribute[i].attribute_str);

				LOG_ERROR("%s: could not find attribute in key and ciphertext (k=%d,c=%d)", __func__, index_key, index_ciph);
				/*DEBUG_ELEMENT_PRINTF("attribute(%s)=%B\n", attribute_list.attribute[i].attribute_str, attribute_list.attribute[i].attribute_hash);*/
				result = FENC_ERROR_INVALID_INPUT;
				goto cleanup;
			}
			
			/* Compute prodONE *= CONE[index_ciph]^{coefficient[i]} */
			element_pow_zn(tempONE, ciphertext_WatersCP.CONE[index_ciph], coefficient_list.coefficients[i].coefficient);
			element_mul(temp2ONE, tempONE, prodONE);
			element_set(prodONE, temp2ONE);
			
			/* Compute finalT = e(KXONE[index_key]^{coefficient[i]}, DTWO[index_ciph]).		*/
			element_pow_zn(tempONE, key_WatersCP->KXONE[index_key], coefficient_list.coefficients[i].coefficient);
			pairing_apply(finalT, tempONE, ciphertext_WatersCP.DTWO[index_ciph], scheme_context->global_params->pairing);
			
			/* We computed the intermediate value as "finalT".
			 * Now compute prodT *= finalT.	*/
			element_mul(temp2GT, finalT, prodT);
			element_set(prodT, temp2GT);
		} /* end of if coefficient.is_set clause */
	} /* end of for clause */
	
	/* Now compute prodT *= e(prodONE, LTWO).	*/
	pairing_apply(finalT, prodONE, key_WatersCP->LTWO, scheme_context->global_params->pairing);
	element_mul(temp2GT, finalT, prodT);
	element_set(prodT, temp2GT);

	/* Final computation: prodT = e(CprimeONE, KTWO) / prodT.	*/
	pairing_apply(tempGT, ciphertext_WatersCP.CprimeONE, key_WatersCP->KTWO, scheme_context->global_params->pairing);
	element_invert(temp2GT, prodT);
	element_mul(prodT, tempGT, temp2GT);

	/* Finally, hash this result to obtain the KEM decryption.  Full encryption is for the future.	*/
	if (ciphertext_WatersCP.type == FENC_CIPHERTEXT_TYPE_KEM_CPA) {
		/* If its a KEM, hash prodT and that's the resulting session key.	*/
		err_code = derive_session_key_from_element(plaintext, prodT, ciphertext_WatersCP.kem_key_len, scheme_context->global_params->pairing);
		if (err_code != FENC_ERROR_NONE) {
			result = err_code;
			goto cleanup;
		}
	} else {
		LOG_ERROR("%s: only KEM mode is supported at this point", __func__);
		result = FENC_ERROR_NOT_IMPLEMENTED;
		goto cleanup;
	}

	/* Success!	*/
	result = FENC_ERROR_NONE;

cleanup:
	/* If the coefficient list was allocated/initialized, we have to clear it.	*/
	if (coefficients_initialized == TRUE) {
		LSSS_clear_coefficients_list(&coefficient_list);
	}
	
	/* Clear temporary variables.	*/
	if (elements_initialized == TRUE) {
		element_clear(finalT);
		element_clear(prodT);
		element_clear(tempGT);
		element_clear(temp2GT);	
		element_clear(temp3GT);	
		element_clear(temp4GT);	
		element_clear(prodONE);
		element_clear(tempONE);
		element_clear(temp2ONE);	
		element_clear(temp3ONE);	
		element_clear(tempZ);	
		element_clear(temp2Z);	
	}
	
	/* Clear out the attribute_list structure.	*/
	if (attribute_list_N_initialized == TRUE) {
		fenc_attribute_list_clear(&(attribute_list));
	}
	
	return result;
}

FENC_ERROR
libfenc_retrieve_attribute_policy(fenc_context *context, fenc_ciphertext *ciphertext, uint8 *buffer, size_t buf_len)
{
	FENC_ERROR result = FENC_ERROR_NONE, err_code;
	fenc_ciphertext_WatersCP		ciphertext_WatersCP;
	fenc_scheme_context_WatersCP	*scheme_context = NULL;
	
	/* Get the scheme-specific context. */
	scheme_context = (fenc_scheme_context_WatersCP*)context->scheme_context;
	if (scheme_context == NULL) {
		return FENC_ERROR_INVALID_CONTEXT;
	}
		
	/* Deserialize the ciphertext.	*/
	err_code = libfenc_deserialize_ciphertext_WatersCP(ciphertext->data, ciphertext->data_len, &ciphertext_WatersCP, scheme_context);
	if (err_code != FENC_ERROR_NONE) {
		LOG_ERROR("%s: unable to deserialize ciphertext", __func__);
		result = err_code;
		return FENC_ERROR_INVALID_CIPHERTEXT;
	}
	
	if(strlen(ciphertext_WatersCP.policy_str) < buf_len) {
		memcpy(buffer, ciphertext_WatersCP.policy_str, strlen(ciphertext_WatersCP.policy_str));
	}
	return FENC_ERROR_NONE;
}

/*!
 * Internal function for computing a ciphertext.  In key-encapsulation mode this function
 * returns a key and a buffer.  In standard mode it encrypts a given plaintext.
 *
 * @param context		The fenc_context data structure
 * @param input			The function input under which which the ciphertext will be encrypted.
 * @param plaintext		The plaintext message.
 * @param kem_mode		Set to "TRUE" if using KEM mode, false for normal encryption.
 * @param kem_key_buf	Buffer for the returned session key (KEM mode only).
 * @param kem_key_len	Pointer to a key length; input is desired, overwritten with actual length.
 * @param ciphertext	A pre-allocated buffer for the returned fenc_ciphertext.
 * @return				FENC_ERROR_NONE or an error code.
 */

FENC_ERROR
encrypt_WatersCP_internal(fenc_context *context, fenc_function_input *input, fenc_plaintext *plaintext,
					 Bool kem_mode, uint8* kem_key_buf, size_t kem_key_len, fenc_ciphertext *ciphertext)
{
	FENC_ERROR result = FENC_ERROR_UNKNOWN, err_code = FENC_ERROR_NONE;
	fenc_attribute_policy *policy = NULL;
	fenc_scheme_context_WatersCP* scheme_context = NULL;
	fenc_ciphertext_WatersCP ciphertext_WatersCP;
	element_t rZ, sZ, eggalphasT, tempZ;
	element_t tempONE, temp2ONE;
	element_t sxZ[MAX_CIPHERTEXT_ATTRIBUTES];
	uint32 i;
	Bool elements_initialized = FALSE;
	size_t serialized_len = 0;
	fenc_attribute_list attribute_list;
	// char temp_policy_str[MAX_POLICY_STR];

	/* Get the scheme-specific context. */
	scheme_context = (fenc_scheme_context_WatersCP*)context->scheme_context;
	if (scheme_context == NULL) {
		result = FENC_ERROR_INVALID_CONTEXT;
		goto cleanup;
	}
	
	/* Parse the function input as an attribute policy.  This will allocate memory
	 * that will ultimately be released when the key is cleared.				*/
	policy = (fenc_attribute_policy*)SAFE_MALLOC(sizeof(fenc_attribute_policy));
	err_code = libfenc_parse_input_as_attribute_policy(input, policy);
	if (err_code != FENC_ERROR_NONE) {
		LOG_ERROR("%s: could not parse function input as policy", __func__);
		result = FENC_ERROR_INVALID_INPUT;
		goto cleanup;
	}
	
	/* Initialize temporary elements.	*/
	element_init_Zr(rZ, scheme_context->global_params->pairing);
	element_init_Zr(sZ, scheme_context->global_params->pairing);
	element_init_GT(eggalphasT, scheme_context->global_params->pairing);
	element_init_G1(tempONE, scheme_context->global_params->pairing);
	element_init_G1(temp2ONE, scheme_context->global_params->pairing);
	elements_initialized = TRUE;
	
	/* Select sZ and compute eggalphaZT = eggalphaT^{sZ}.	*/
	element_random(sZ);
	element_pow_zn(eggalphasT, scheme_context->public_params.eggalphaT, sZ);
	
	/* Export the policy to a string and draw it back in again.  This clears up some issues in the way		*/
	//strcpy(temp_policy_str, "");
	//err_code = fenc_attribute_policy_to_string(policy->root, temp_policy_str, MAX_POLICY_STR);
	//if (err_code != FENC_ERROR_NONE) {
	//	LOG_ERROR("%s: could not serialize policy", __func__);
	//	result = err_code;
	//	goto cleanup;
	//}
	// fenc_policy_from_string(policy, temp_policy_str);
	
	// printf("Original policy string: '%s'\n", policy->string);
	//strcpy(temp_policy_str, "");
	//err_code = fenc_attribute_policy_to_string(policy->root, temp_policy_str, MAX_POLICY_STR);
	//printf("Revised policy string: %s\n", temp_policy_str);
	
		
	/* Use the Linear Secret Sharing Scheme (LSSS) to compute an enumerated list of all
	 * attributes and corresponding secret shares of sZ.  The shares will be placed into 
	 * a fenc_attribute_list structure that we'll embed within the fenc_key_WatersCP struct.	*/
	memset(&attribute_list, 0, sizeof(fenc_attribute_list));
	err_code = fenc_LSSS_calculate_shares_from_policy(&(sZ), policy, &attribute_list, 
													  scheme_context->global_params->pairing);
	if (err_code != FENC_ERROR_NONE) {
		LOG_ERROR("%s: could not calculate shares", __func__);
		result = FENC_ERROR_INVALID_INPUT;
		goto cleanup;
	}
	
	/* Initialize the WatersCP-specific ciphertext data structure and allocate some temporary variables.	*/
	err_code = fenc_ciphertext_WatersCP_initialize(&ciphertext_WatersCP, &attribute_list, policy, FENC_CIPHERTEXT_TYPE_KEM_CPA,
													scheme_context);
	if (err_code != FENC_ERROR_NONE) {
		LOG_ERROR("%s: could not initialize ciphertext structure", __func__);
		result = FENC_ERROR_UNKNOWN;
		goto cleanup;
	}
	//strcpy(ciphertext_WatersCP.policy_str, temp_policy_str);
	strcpy(ciphertext_WatersCP.policy_str, fenc_get_policy_string(policy));

	/* If we're in KEM mode, the returned key is the hash of eggalphasT.	*/
	if (kem_mode == TRUE) {
		err_code = fenc_derive_key_from_element(eggalphasT, kem_key_len, kem_key_buf);
		if (err_code != FENC_ERROR_NONE) {
			result = err_code;
			goto cleanup;
		}
		ciphertext_WatersCP.type = FENC_CIPHERTEXT_TYPE_KEM_CPA;
		ciphertext_WatersCP.kem_key_len = kem_key_len;
	} else {
		/* Not in KEM mode.	*/
		LOG_ERROR("%s: KEM mode is currently the only supported form of encryption.", __func__);
		result = FENC_ERROR_UNKNOWN;
		goto cleanup;
	}
	
	/* Compute CprimeONE = gONE^{sZ}.	*/
	element_pow_zn(ciphertext_WatersCP.CprimeONE, scheme_context->public_params.gONE, sZ);

	/* For every share/attribute, create one component of the secret key.	*/
	for (i = 0; i < ciphertext_WatersCP.attribute_list.num_attributes; i++) {		
		/* Hash the attribute string to Zr, if it hasn't already been.	*/
		hash_attribute_string_to_Zr(&(ciphertext_WatersCP.attribute_list.attribute[i]), scheme_context->global_params->pairing);
		
		/* Pick a random value r_i (rZ).	*/
		element_random(rZ);
		/* Set DTWO[i] = gTWO^{rZ}.	*/ // very expensive
		element_pow_zn(ciphertext_WatersCP.DTWO[i], scheme_context->public_params.gTWO, rZ);
				
		/* Hash the attribute (already hashed to Zr) into an element of G1 (tempONE).	*/
		err_code = hash2_attribute_element_to_G1(&(ciphertext_WatersCP.attribute_list.attribute[i].attribute_hash), &tempONE);	/* result in tempONE  */
		
		element_pow_zn(temp2ONE, tempONE, rZ);		/* temp2ONE = H(attribute)^{rZ}	*/
		element_invert(tempONE, temp2ONE);			/* tempONE = H(attribute)^{-rZ} */
			
		/* Set CONE[i] = gaONE^{share_i} * H(attribute)^{-rZ}.	*/
		DEBUG_ELEMENT_PRINTF("share %d is %B\n", i, attribute_list.attribute[i].share);
		element_pow_zn(temp2ONE, scheme_context->public_params.gaONE, attribute_list.attribute[i].share);	/* temp2ONE = gaONE^{share_i}		*/
		element_mul(ciphertext_WatersCP.CONE[i], tempONE, temp2ONE);											/* CONE = tempONE * temp2ONE.	*/
	}

	/* DEBUG: Print out the ciphertext.	*/
	// libfenc_fprint_ciphertext_WatersCP(&ciphertext_WatersCP, stdout);
	/* Serialize the WatersCP ciphertext structure into a fenc_ciphertext container 
	 * (which is essentially just a binary buffer).  First we get the length, then we 
	 * allocate the ciphertext buffer, then we serialize.	*/
	libfenc_serialize_ciphertext_WatersCP(&ciphertext_WatersCP, NULL, 0, &serialized_len);	/* This gets the serialized length. */
	libfenc_ciphertext_initialize(ciphertext, serialized_len, FENC_SCHEME_WATERSCP);
	if (err_code != FENC_ERROR_NONE) {	result = err_code;	goto cleanup;	}
	err_code = libfenc_serialize_ciphertext_WatersCP(&ciphertext_WatersCP, ciphertext->data, ciphertext->max_len, &ciphertext->data_len);	/* Serialization. */
	if (err_code != FENC_ERROR_NONE) {	result = err_code;	goto cleanup;	}
		
	/* Success!		*/
	result = FENC_ERROR_NONE;
	
cleanup:
	/* If there was an error, clean up after ourselves.	*/
	
	/* Wipe out temporary variables.	*/
	if (elements_initialized == TRUE) {
		element_clear(rZ);
		element_clear(sZ);
		element_clear(eggalphasT);
		element_clear(tempONE);
		element_clear(temp2ONE);
	}
	
	return result;
}

/*!
 * Export the public parameters (MPK) to a binary buffer.  Calling this function with buffer 
 * set to NULL will return the length of the exported material.
 *
 * @param context		The fenc_context data structure
 * @param buffer		A pre-allocated buffer for the resulting export.
 * @param max_len		The maximum allocated size of the buffer (in bytes).
 * @param result_len	The size of the resulting export (in bytes).
 * @return				FENC_ERROR_NONE or an error code.
 */

FENC_ERROR	libfenc_export_public_params_WatersCP(fenc_context *context, uint8 *buffer, size_t max_len, size_t *result_len)
{
	FENC_ERROR err_code;
	fenc_scheme_context_WatersCP* scheme_context;
	
	/* Get the scheme-specific context. */
	scheme_context = (fenc_scheme_context_WatersCP*)context->scheme_context;
	
	/* Export the elements to the buffer.  Note that if buffer is NULL this routine will
	 * just compute the necessary buffer length.									*/
	err_code = export_components_to_buffer(buffer, max_len, result_len, "%C%C%C%C%E",
									   &(scheme_context->public_params.gONE), 
									   &(scheme_context->public_params.gTWO),
									   &(scheme_context->public_params.gaONE),
									   &(scheme_context->public_params.gaTWO),
									   &(scheme_context->public_params.eggalphaT));
	
	return err_code;
}	

/*!
 * Export a context's secret parameters (MSK) to a binary buffer.  Calling this function with buffer 
 * set to NULL will return the length of the exported material.
 *
 * @param context		The fenc_context data structure
 * @param buffer		A pre-allocated buffer for the resulting export.
 * @param max_len		The maximum allocated size of the buffer (in bytes).
 * @param result_len	The size of the resulting export (in bytes).
 * @return				FENC_ERROR_NONE or an error code.
 */

FENC_ERROR
libfenc_export_secret_params_WatersCP(fenc_context *context, uint8 *buffer, size_t max_len, size_t *result_len)
{
	fenc_scheme_context_WatersCP* scheme_context;
	unsigned char *buf_ptr = (unsigned char*)buffer;
	
	/* Get the scheme-specific context. */
	scheme_context = (fenc_scheme_context_WatersCP*)context->scheme_context;
	
	/* Export the elements to the buffer.  Note that if buffer is NULL this routine will
	 * just compute the necessary buffer length.									*/
	return export_components_to_buffer(buffer, max_len, result_len, "%E",
									   &(scheme_context->secret_params.alphaZ));
}	

/*!
 * Import the public parameters (MPK) from a binary buffer.
 *
 * @param context		The fenc_context data structure
 * @param buffer		The buffer.
 * @param max_len		The size of the buffer (in bytes).
 * @return				FENC_ERROR_NONE or an error code.
 */

FENC_ERROR
libfenc_import_public_params_WatersCP(fenc_context *context, uint8 *buffer, size_t buf_len, fenc_global_params *global_params)
{
	//FENC_ERROR err_code;
	fenc_scheme_context_WatersCP* scheme_context;
	//size_t bytes_read = 0;
	
	scheme_context = (fenc_scheme_context_WatersCP*)context->scheme_context;
	if (scheme_context == NULL) {
		return FENC_ERROR_INVALID_CONTEXT;
	}
	
	/* Sanity check: Make sure that we have initialized group/global parameters.		*/
	if (scheme_context->global_params == NULL) {
		LOG_ERROR("%s: global/group parameters are not set", __func__);
		return FENC_ERROR_INVALID_GLOBAL_PARAMS;
	}
	
	/* Initialize the public parameters, allocating group elements.		*/
	public_params_initialize_WatersCP(&(scheme_context->public_params), scheme_context->global_params->pairing);

	/* Import the elements from the buffer.								*/
	return import_components_from_buffer(buffer, buf_len, NULL, "%C%C%C%C%E",
										 &(scheme_context->public_params.gONE), 
										 &(scheme_context->public_params.gTWO),
										 &(scheme_context->public_params.gaONE),
										 &(scheme_context->public_params.gaTWO),
										 &(scheme_context->public_params.eggalphaT));
}

/*!
 * Import the secret parameters (MPK) from a binary buffer.
 *
 * @param context		The fenc_context data structure
 * @param buffer		The buffer.
 * @param max_len		The size of the buffer (in bytes).
 * @return				FENC_ERROR_NONE or an error code.
 */

FENC_ERROR
libfenc_import_secret_params_WatersCP(fenc_context *context, uint8 *buffer, size_t buf_len)
{
	// FENC_ERROR err_code;
	fenc_scheme_context_WatersCP* scheme_context;
	
	scheme_context = (fenc_scheme_context_WatersCP*)context->scheme_context;
	if (scheme_context == NULL) {
		return FENC_ERROR_INVALID_CONTEXT;
	}
	
	/* Initialize the secret parameters, allocating group elements.		*/
	secret_params_initialize_WatersCP(&(scheme_context->secret_params), scheme_context->global_params->pairing);
	
	return import_components_from_buffer(buffer, buf_len, NULL, "%E",
										 &(scheme_context->secret_params.alphaZ));
}


/*!
 * Import the global parameters (MPK) from a binary buffer.
 *
 * @param context		The fenc_context data structure
 * @param buffer		The buffer.
 * @param max_len		The size of the buffer (in bytes).
 * @return				FENC_ERROR_NONE or an error code.
 */

FENC_ERROR
libfenc_import_global_params_WatersCP(fenc_context *context, uint8 *buffer, size_t buf_len)
{
	FENC_ERROR err_code;
	fenc_scheme_context_WatersCP* scheme_context;
	size_t bytes_read = 0;
	fenc_group_params group_params;
	
	scheme_context = (fenc_scheme_context_WatersCP*)context->scheme_context;
	if (scheme_context == NULL) {
		return FENC_ERROR_INVALID_CONTEXT;
	}
	
	/* Read the global parameters out of the buffer, if they're in there.	*/
	err_code = libfenc_load_group_params_from_buf(&(group_params), buffer, buf_len);
	if (err_code != FENC_ERROR_NONE) {
		LOG_ERROR("%s: could not read group params", __func__);
		return FENC_ERROR_INVALID_GLOBAL_PARAMS;
	}
	
	/* Initialize the scheme's global parameters.	*/
	scheme_context->global_params = initialize_global_params_WatersCP(&group_params, scheme_context->global_params);
	
	return err_code;
}

/*!
 * Export the global parameters to a binary buffer.  Calling this function with buffer 
 * set to NULL will return the length of the exported material.
 *
 * @param context		The fenc_context data structure
 * @param buffer		A pre-allocated buffer for the resulting export.
 * @param max_len		The maximum allocated size of the buffer (in bytes).
 * @param result_len	The size of the resulting export (in bytes).
 * @return				FENC_ERROR_NONE or an error code.
 */

FENC_ERROR	
libfenc_export_global_params_WatersCP(fenc_context *context, uint8 *buffer, size_t max_len, size_t *result_len)
{
	FENC_ERROR err_code;
	fenc_scheme_context_WatersCP* scheme_context;
	size_t params_len;
	
	/* Get the scheme-specific context. */
	scheme_context = (fenc_scheme_context_WatersCP*)context->scheme_context;
	
	/* Export the group parameters to the buffer.  If th buffer is NULL this only compute the length.		*/
	err_code = libfenc_export_group_params(&(scheme_context->global_params->group_params), buffer, max_len, result_len);
	
	return err_code;
}	

/*!
 * Serialize an ABE key structure.
 *
 * @param context		The fenc_context data structure
 * @param key			The fenc_key data structure.
 * @param buffer		A pre-allocated buffer for the resulting export.
 * @param buf_len		The maximum allocated size of the buffer (in bytes).
 * @param result_len	The size of the resulting export (in bytes).
 * @return				FENC_ERROR_NONE or an error code.
 */

FENC_ERROR	
libfenc_export_secret_key_WatersCP(fenc_context *context, fenc_key *key, uint8 *buffer, size_t buf_len, size_t *result_len)
{
	FENC_ERROR err_code = FENC_ERROR_NONE;

	/* retrieve the key for the WatersCP context */
	fenc_key_WatersCP *key_WatersCP = (fenc_key_WatersCP *) key->scheme_key;
	if(key_WatersCP == NULL) {
		err_code = FENC_ERROR_INVALID_INPUT;
		LOG_ERROR("%s: fenc_key not existent.", __func__);
		goto cleanup;
	}
	
	/* serialize key structure to buffer */
	err_code = libfenc_serialize_key_WatersCP(key_WatersCP, buffer, buf_len, result_len);		
	if (err_code != FENC_ERROR_NONE) {
		err_code = FENC_ERROR_INVALID_INPUT;
		LOG_ERROR("%s: cannot serialize key to buffer. has key been constructed?", __func__);
		goto cleanup;
	}
	
cleanup:	
	return err_code;
}

/*!
 * Deserialize an ABE key structure.
 *
 * @param context		The fenc_context data structure
 * @param key			The fenc_key data structure (pre-allocated), but not initialized.
 * @param buffer		The buffer which contains the binary contents of key?
 * @param buf_len		The size of the buffer (in bytes).
 * @return				FENC_ERROR_NONE or an error code.
 */

FENC_ERROR	
libfenc_import_secret_key_WatersCP(fenc_context *context, fenc_key *key, uint8 *buffer, size_t buf_len)
{
	FENC_ERROR err_code = FENC_ERROR_NONE;
	fenc_key_WatersCP			*key_WatersCP = NULL;
	fenc_attribute_list			*attribute_list;
	uint32						num_components;
	size_t						import_len;
	fenc_scheme_context_WatersCP *scheme_context;

	/* Get the scheme-specific context. */
	scheme_context = (fenc_scheme_context_WatersCP*)context->scheme_context;
	if (scheme_context == NULL) {
		return FENC_ERROR_INVALID_CONTEXT;
	}
	
	/* Allocate an attribute list data structure.	*/
	attribute_list = (fenc_attribute_list*) SAFE_MALLOC(sizeof(fenc_attribute_list));
	if (attribute_list == NULL) {
		LOG_ERROR("%s: could not allocate attribute list", __func__);
		return FENC_ERROR_OUT_OF_MEMORY;
	}	
	
	/* import attributes only -- should be first in buffer */
	err_code = import_components_from_buffer(buffer, buf_len, &import_len, "%A%d",
											 attribute_list,
											 &(num_components));
	/* sanity check */
	if(num_components != attribute_list->num_attributes) {
		LOG_ERROR("%s: mis-match in attributes found in key", __func__);
		err_code = FENC_ERROR_INVALID_INPUT;
		goto cleanup;
	}
	// printf("import_len => '%zu'\n", import_len);
	
	/* Initialize the WatersCP-specific key data structure and allocate some temporary variables.	*/
	key_WatersCP = fenc_key_WatersCP_initialize(attribute_list, FALSE, scheme_context->global_params);
	if (key_WatersCP == NULL) {
		LOG_ERROR("%s: could not initialize key structure", __func__);
		return FENC_ERROR_INVALID_INPUT;
	}
		
	/* deserialize remaining buffer into key_watersCP structure -- KTWO, LONE, and KXONE (num_component times) */
	err_code = libfenc_deserialize_key_WatersCP(key_WatersCP, (uint8 *) (buffer + import_len), (buf_len - import_len));
	if (err_code != FENC_ERROR_NONE) {
		LOG_ERROR("%s: could not deserialize into key structure", __func__);
		err_code = FENC_ERROR_INVALID_INPUT;
		goto cleanup;
	}
	
	/* Stash the key_WatersCP structure inside of the fenc_key.		*/
	memset(key, 0, sizeof(fenc_key));
	key->scheme_type = FENC_SCHEME_WATERSCP;
	key->valid = TRUE;
	key->scheme_key = (void*)key_WatersCP;
		
	return err_code;

cleanup:
	/* clean up here */
	free(attribute_list);
	
	return FENC_ERROR_NONE;
}

/**************************************************************************************
 * Utility functions
 **************************************************************************************/
	
/*!
 * Destroy the internal contents of a fenc_context structure.  The caller is responsible for
 * de-allocating the context buffer itself.
 *
 * @param context		The fenc_context data structure.
 * @return				FENC_ERROR_NONE or an error code.
 */

FENC_ERROR	
libfenc_destroy_context_WatersCP(fenc_context *context)
{
	FENC_ERROR result = FENC_ERROR_UNKNOWN;
	fenc_scheme_context_WatersCP *scheme_context;
	
	scheme_context = (fenc_scheme_context_WatersCP*)context->scheme_context;
	
	/* Destroy the scheme-specific context structure */
	if (scheme_context != NULL) {
		/* Destroy the internal global parameters.	*/
		if (scheme_context->global_params != NULL) {
			SAFE_FREE(scheme_context->global_params);
		}
		
		memset(context->scheme_context, 0, sizeof(fenc_scheme_context_WatersCP) );
		SAFE_FREE(context->scheme_context);
	}
	
	/* Other destruction operations go here... */
	result = FENC_ERROR_NONE;
	
	return result;
}

/*!
 * Destroy the internal contents of a fenc_context structure.  The caller is responsible for
 * de-allocating the context buffer itself.
 *
 * @param global_params	The global parameters.
 * @return				FENC_ERROR_NONE or an error code.
 */

FENC_ERROR
libfenc_destroy_global_params_WatersCP(fenc_global_params *global_params)
{
	return FENC_ERROR_NOT_IMPLEMENTED;
}

/*!
 * Validate a set of global parameters for the LSW scheme.
 *
 * @param global_params	The global parameters.
 * @return				FENC_ERROR_NONE or an error code.
 */

FENC_ERROR
libfenc_validate_global_params_WatersCP(fenc_global_params *global_params)
{
	FENC_ERROR result;
	
	/* Sanity check -- make sure the global_params exist. */
	if (global_params == NULL) {
		return FENC_ERROR_INVALID_GLOBAL_PARAMS;
	}
	
	/* Utility call --- check that bilinear group parameters have
	 * been loaded into global_params.  We might someday want to require
	 * a specific class of group parameters, but for the moment we're ok. */
	result = libfenc_validate_group_params(global_params->group_params);
	
	/* Since there are no other global parameters in the LSW scheme, we're done. */
	return result;
}

/*!
 * Serialize a decryption key to a binary buffer.  Accepts a WatersCP key, buffer, and buffer length.
 * If the buffer is large enough, the serialized result is written to the buffer and returns the
 * length in "serialized_len".  Calling with a NULL buffer returns the length /only/ but does
 * not actually serialize the structure.
 *
 * @param key				The key to serialize.
 * @param buffer			Pointer to a buffer, or NULL to get the length only.
 * @param max_len			The maximum size of the buffer (in bytes).
 * @param serialized_len	Total size of the serialized structure (in bytes).
 * @return					FENC_ERROR_NONE or FENC_ERROR_BUFFER_TOO_SMALL.
 */

FENC_ERROR
libfenc_serialize_key_WatersCP(fenc_key_WatersCP *key, unsigned char *buffer, size_t max_len, size_t *serialized_len)
{
	FENC_ERROR err_code = FENC_ERROR_NONE;
	unsigned char *buf_ptr = (unsigned char*)buffer;
//	char *policy_str;
//	size_t str_index = 0, str_len = MAX_POLICY_STR - 1, 
	size_t result_len = 0;
	uint32 i;
	
	/* Export the attribute list and the number of components in the key, along with KTWO and LTWO.	*/
    err_code = export_components_to_buffer(buf_ptr, max_len, &result_len, "%A%d%C%C",									   
										   &(key->attribute_list),										   
										   key->num_components,
										   key->KTWO,
										   key->LTWO);
//	printf("result len = %zd\n", result_len);
	if (err_code != FENC_ERROR_NONE) {
		return err_code;
	}
	/* set serialized_len */
	*serialized_len = 0;
	*serialized_len += result_len;
	if (buffer != NULL) {	buf_ptr = buffer + *serialized_len;	}
	max_len -= result_len;
	
	/* Now output each component KXONE of the key.								*/
	for (i = 0; i < key->num_components; i++) {
		/* Export the group element.	*/
		err_code = export_components_to_buffer(buf_ptr, max_len, &result_len, "%C",
											   &(key->KXONE[i]));		
		if (err_code != FENC_ERROR_NONE) {
			return err_code;
		}
		
		*serialized_len += result_len;
		if (buffer != NULL) {	buf_ptr = buffer + *serialized_len;	}
		max_len -= result_len;
	}
	
	/* All done.	*/
	return err_code;
}

/*!
 * Deserialize a decryption key from a binary buffer.  Accepts an LSW key, buffer, and buffer length.
 * If the buffer is large enough, the serialized result is written to the buffer and returns the
 * length in "serialized_len".  Calling with a NULL buffer returns the length /only/ but does
 * not actually serialize the structure.
 *
 * @param key				The key to serialize.
 * @param buffer			Pointer to a buffer, or NULL to get the length only.
 * @param buf_len			The length of the buffer (in bytes).
 * @param serialized_len	Total size of the serialized structure (in bytes).
 * @return					FENC_ERROR_NONE or FENC_ERROR_BUFFER_TOO_SMALL.
 */

FENC_ERROR
libfenc_deserialize_key_WatersCP(fenc_key_WatersCP *key, unsigned char *buffer, size_t buf_len)
{
	FENC_ERROR err_code = FENC_ERROR_NONE;
	unsigned char *buf_ptr = (unsigned char*)buffer;
//	char *policy_str;
//	size_t str_index = 0, str_len = MAX_POLICY_STR - 1, 
	size_t result_len = 0;
	uint32 i;

	/* Import the attribute list and the number of components in the key, along with KTWO and LTWO.	*/
	err_code = import_components_from_buffer(buf_ptr, buf_len, &result_len, "%C%C",
//										   &(key->attribute_list),
//										   &(key->num_components),
										   &(key->KTWO),
										   &(key->LTWO));
	if (err_code != FENC_ERROR_NONE) {
		return err_code;
	}
	buf_ptr = buffer + result_len;
	buf_len -= result_len;
	
	/* Now import each component KXONE of the key.								*/
	for (i = 0; i < key->num_components; i++) {
		/* Import the group element.	*/
		err_code = import_components_from_buffer(buf_ptr, buf_len, &result_len, "%C",
											   &(key->KXONE[i]));		
		if (err_code != FENC_ERROR_NONE) {
			return err_code;
		}
		
		if (buffer != NULL) {	buf_ptr += result_len;	}
		buf_len -= result_len;
	}
	
	/* All done.	*/
	return err_code;
}

/*!
 * Serialize a ciphertext to a binary buffer.  Accepts an LSW ciphertext, buffer, and buffer length.
 * If the buffer is large enough, the serialized result is written to the buffer and returns the
 * length in "serialized_len".  Calling with a NULL buffer returns the length /only/ but does
 * not actually serialize the structure.
 *
 * @param ciphertext		The ciphertext to serialize.
 * @param buffer			Pointer to a buffer, or NULL to get the length only.
 * @param max_len			The maximum size of the buffer (in bytes).
 * @param serialized_len	Total size of the serialized structure (in bytes).
 * @return					FENC_ERROR_NONE or FENC_ERROR_BUFFER_TOO_SMALL.
 */

FENC_ERROR
libfenc_serialize_ciphertext_WatersCP(fenc_ciphertext_WatersCP *ciphertext, unsigned char *buffer, size_t max_len, size_t *serialized_len)
{
	int i;
	unsigned char *buf_ptr = (unsigned char*)buffer;
	uint32 type, kem_key_len;
	
	
	/* First, compute the length (in bytes) of the serialized ciphertext, then (if buffer is non-null)
	 * and there's sufficient room, serialize the value into the buffer. */
	*serialized_len = 0;
	*serialized_len += sizeof(uint32);												/* ciphertext type	*/
	if (buffer != NULL && *serialized_len <= max_len) {
		type = ciphertext->type;
		EXPORT_INT32(buf_ptr, (uint32)type);
		buf_ptr = buffer + *serialized_len;
	}
	
	if (ciphertext->type == FENC_CIPHERTEXT_TYPE_KEM_CPA)	{						/* KEM session key size */
		*serialized_len += sizeof(uint32);												/* only in KEM mode	*/
		if (buffer != NULL && *serialized_len <= max_len) {
			kem_key_len = ciphertext->kem_key_len;
			EXPORT_INT32(buf_ptr, kem_key_len);
			buf_ptr = buffer + *serialized_len;
		}
	}
	
	*serialized_len += sizeof(ciphertext->attribute_list.num_attributes);			/* num_attributes	*/
	if (buffer != NULL && *serialized_len <= max_len) {
		EXPORT_INT32(buf_ptr, ciphertext->attribute_list.num_attributes);
		buf_ptr = buffer + *serialized_len;
	}
	
		
	*serialized_len += strlen(ciphertext->policy_str) + 1;							/* policy string + NULL terminator	*/
	if (buffer != NULL && *serialized_len <= max_len) {
		sprintf((char*)buf_ptr, "%s", ciphertext->policy_str);
		buf_ptr = buffer + *serialized_len;
	}
	
	if (ciphertext->type == FENC_CIPHERTEXT_TYPE_CPA)	{
		*serialized_len += element_length_in_bytes(ciphertext->CT);					/* CT	(skipped in KEM mode!)	*/
		if (buffer != NULL && *serialized_len <= max_len) {
			element_to_bytes(buf_ptr, ciphertext->CT);
			buf_ptr = buffer + *serialized_len;
		}
	}
	
	*serialized_len += element_length_in_bytes_compressed(ciphertext->CprimeONE);	/* CprimeONE			*/
	if (buffer != NULL && *serialized_len <= max_len) {
		element_to_bytes_compressed(buf_ptr, ciphertext->CprimeONE);
		buf_ptr = buffer + *serialized_len;
	}

	/* For every attribute in the ciphertext... */
	for (i = 0; i < ciphertext->attribute_list.num_attributes; i++) {
		*serialized_len += element_length_in_bytes(ciphertext->attribute_list.attribute[i].attribute_hash);			/* attribute[i]		*/
		if (buffer != NULL && *serialized_len <= max_len) {
			element_to_bytes(buf_ptr, ciphertext->attribute_list.attribute[i].attribute_hash);
			buf_ptr = buffer + *serialized_len;
		}
		
		*serialized_len += element_length_in_bytes(ciphertext->CONE[i]);		/* CONE[i]		*/
		if (buffer != NULL && *serialized_len <= max_len) {
			element_to_bytes(buf_ptr, ciphertext->CONE[i]);
			buf_ptr = buffer + *serialized_len;
		}
		
		*serialized_len += element_length_in_bytes(ciphertext->DTWO[i]);		/* DTWO[i]		*/
		if (buffer != NULL && *serialized_len <= max_len) {
			element_to_bytes(buf_ptr, ciphertext->DTWO[i]);
			buf_ptr = buffer + *serialized_len;
		}
	}
	
	/* If the buffer pointer is NULL, we're done --- just return the length. */
	if (buffer == NULL) {
		return FENC_ERROR_NONE;
	}
	
	/* If the serialized length was too large for the buffer, return an error. */
	if (*serialized_len > max_len) {
		return FENC_ERROR_BUFFER_TOO_SMALL;
	}
	
	/* Return success. */
	return FENC_ERROR_NONE;
}

/*!
 * Deserialize a ciphertext from a binary buffer.  Accepts a buffer and buffer length and
 * transcribes the result into an LSW ciphertext data structure.  
 *
 * Note: this routine uses deserialization functionality from the PBC library; this could
 * fail catastrophically when given an invalid ciphertext.
 *
 * @param buffer			Pointer to a buffer from which to deserialize.
 * @param buf_len			The size of the buffer (in bytes).
 * @param ciphertext		The fenc_ciphertext_WatersCP structure.
 * @param scheme_context	The scheme context which contains the group parameters.
 * @return					FENC_ERROR_NONE or FENC_ERROR_BUFFER_TOO_SMALL.
 */

FENC_ERROR
libfenc_deserialize_ciphertext_WatersCP(unsigned char *buffer, size_t buf_len, fenc_ciphertext_WatersCP *ciphertext, fenc_scheme_context_WatersCP *scheme_context)
{
	size_t i;
	size_t deserialized_len;
	uint32 num_attributes, type, kem_key_len;
	fenc_attribute_policy *policy = NULL;
	fenc_attribute_list attribute_list;
	FENC_ERROR result = FENC_ERROR_UNKNOWN, err_code;
	unsigned char *buf_ptr = buffer;
	
	deserialized_len = 0;
	deserialized_len += sizeof(uint32);								/* ciphertext type	*/
	if (deserialized_len <= buf_len) {
		IMPORT_INT32(type, buf_ptr);
		buf_ptr = buffer + deserialized_len;
	}
	
	if (type == FENC_CIPHERTEXT_TYPE_KEM_CPA)	{					/* KEM session key size */
		deserialized_len += sizeof(uint32);							/* only in KEM mode		*/
		if (deserialized_len <= buf_len) {
			IMPORT_INT32(kem_key_len, buf_ptr);
			buf_ptr = buffer + deserialized_len;
		}
	}
	
	deserialized_len += sizeof(num_attributes);						/* num_attributes	*/
	if (deserialized_len <= buf_len) {
		IMPORT_INT32(num_attributes, buf_ptr);
		buf_ptr = buffer + deserialized_len;
	}
	
	/* Sanity check: make sure the number of attributes is non-zero, but not too big. */
	if (num_attributes < 1 || num_attributes > MAX_CIPHERTEXT_ATTRIBUTES) {
		return FENC_ERROR_INVALID_CIPHERTEXT;
	}

	/* Initialize the attribute list.	*/
	err_code = fenc_attribute_list_initialize(&attribute_list, num_attributes);
	if (err_code != FENC_ERROR_NONE) {
		LOG_ERROR("%s: couldn't initialize attribute list", __func__);
		return err_code;
	}
	
	/* Initialize the elements of the WatersCP ciphertext data structure.  This allocates all of the group elements
	 * and sets the num_attributes member.		*/
	err_code = fenc_ciphertext_WatersCP_initialize(ciphertext, &attribute_list, policy, type, scheme_context);
	if (err_code != FENC_ERROR_NONE) {
		/* Couldn't allocate the structure.  Don't even try to cleanup --- this is a really bad situation! */
		LOG_ERROR("%s: couldn't initialize ciphertext", __func__);
		return err_code;
	}
	ciphertext->kem_key_len = kem_key_len;
	
	strncpy((char *)ciphertext->policy_str, (char*)buf_ptr, MAX_POLICY_STR);			/* policy string	*/
	deserialized_len += strlen(ciphertext->policy_str) + 1;				/* TODO: This isn't terribly safe.	*/
	if (deserialized_len <= buf_len) {
		buf_ptr = buffer + deserialized_len;
	}
	
	/* Read in the ciphertext components.								*/	
	if (ciphertext->type == FENC_CIPHERTEXT_TYPE_CPA)	{
		deserialized_len += element_from_bytes(ciphertext->CT, buf_ptr);				/* CT				*/
		if (deserialized_len > buf_len) {											
			result = FENC_ERROR_BUFFER_TOO_SMALL;
			goto cleanup;
		}
		buf_ptr = buffer + deserialized_len;
	}
	
	deserialized_len += element_from_bytes_compressed(ciphertext->CprimeONE, buf_ptr);	/* CprimeONE			*/
	if (deserialized_len > buf_len) {											
		result = FENC_ERROR_BUFFER_TOO_SMALL;
		goto cleanup;
	}
	buf_ptr = buffer + deserialized_len;
	
	/* For every attribute in the ciphertext... */
	for (i = 0; i < ciphertext->attribute_list.num_attributes; i++) {
		memset(&(ciphertext->attribute_list.attribute[i]), 0, sizeof(fenc_attribute));
		element_init_Zr(ciphertext->attribute_list.attribute[i].attribute_hash, scheme_context->global_params->pairing);
		deserialized_len += element_from_bytes(ciphertext->attribute_list.attribute[i].attribute_hash, buf_ptr);			/* attribute[i]		*/
		if (deserialized_len > buf_len) {											
			result = FENC_ERROR_BUFFER_TOO_SMALL;
			goto cleanup;
		}
		ciphertext->attribute_list.attribute[i].is_hashed = TRUE;
		buf_ptr = buffer + deserialized_len;
		
		deserialized_len += element_from_bytes(ciphertext->CONE[i], buf_ptr);	/* CONE[i]			*/
		if (deserialized_len > buf_len) {											
			result = FENC_ERROR_BUFFER_TOO_SMALL;
			goto cleanup;
		}
		buf_ptr = buffer + deserialized_len;
		deserialized_len += element_from_bytes(ciphertext->DTWO[i], buf_ptr);	/* DTWO[i]			*/
		if (deserialized_len > buf_len) {											
			result = FENC_ERROR_BUFFER_TOO_SMALL;
			goto cleanup;
		}
		buf_ptr = buffer + deserialized_len;
	}
	
	/* Success!	*/
	result = FENC_ERROR_NONE;
	
cleanup:
	/* If the deserialization failed, de-allocate any elements we initialized. */
	if (result != FENC_ERROR_NONE) {
		fenc_ciphertext_WatersCP_clear(ciphertext);
	}
	
	/* Return the result. */
	return result;
}

/*!
 * Utility function to allocate the internals of a fenc_ciphertext_WatersCP structure.  
 *
 * @param ciphertext		Pointer to fenc_ciphertext_WatersCP struct.
 * @param num_attributes	Number of attributes.
 * @param scheme_context	Pointer to a fenc_scheme_context_WatersCP struct.
 * @return					FENC_ERROR_NONE or an error code.
 */

FENC_ERROR
fenc_ciphertext_WatersCP_initialize(fenc_ciphertext_WatersCP *ciphertext, fenc_attribute_list *attribute_list, fenc_attribute_policy* policy, FENC_CIPHERTEXT_TYPE type,
							   fenc_scheme_context_WatersCP *scheme_context)
{
	FENC_ERROR err_code = FENC_ERROR_NONE;
	size_t i;
	
	memset(ciphertext, 0, sizeof(fenc_ciphertext_WatersCP));

	/* Copy the attribute list.	*/
	fenc_attribute_list_copy(&(ciphertext->attribute_list), attribute_list, scheme_context->global_params->pairing);
	
	/* Copy the policy into a string.	*/
	if (policy != NULL) {
		strcpy(ciphertext->policy_str, "");
		err_code = fenc_attribute_policy_to_string(policy->root, ciphertext->policy_str, MAX_POLICY_STR);
		
		if (err_code != FENC_ERROR_NONE) {
			LOG_ERROR("%s: policy string is too long", __func__);
			return FENC_ERROR_INVALID_INPUT;
		}
	}
	
	element_init_GT(ciphertext->CT, scheme_context->global_params->pairing);
	element_set1(ciphertext->CT);
	element_init_G1(ciphertext->CprimeONE, scheme_context->global_params->pairing);
	for (i = 0; i < attribute_list->num_attributes; i++) {
		element_init_G1(ciphertext->CONE[i], scheme_context->global_params->pairing);
		element_init_G2(ciphertext->DTWO[i], scheme_context->global_params->pairing);
	}
	ciphertext->type = type;
	
	return FENC_ERROR_NONE;
}

/*!
 * Utility function to release the internals of a fenc_ciphertext_WatersCP structure.  
 *
 * @param ciphertext		Pointer to fenc_ciphertext_WatersCP struct.
 * @return					FENC_ERROR_NONE or an error code.
 */

FENC_ERROR
fenc_ciphertext_WatersCP_clear(fenc_ciphertext_WatersCP *ciphertext)
{
	size_t i;
	
	/* Make sure the number of attributes is reasonable (if not, this is an invalid ciphertext).	*/
	if (ciphertext->attribute_list.num_attributes < 1 || ciphertext->attribute_list.num_attributes > MAX_CIPHERTEXT_ATTRIBUTES) {
		LOG_ERROR("fenc_ciphertext_WatersCP_clear: ciphertext has an invalid number of attributes"); 
		return FENC_ERROR_UNKNOWN;
	}
	
	/* Release all of the internal elements.  Let's hope the ciphertext was correctly inited! */
	element_clear(ciphertext->CT);
	element_clear(ciphertext->CprimeONE);
	for (i = 0; i < ciphertext->attribute_list.num_attributes; i++) {
		element_clear(ciphertext->CONE[i]);
		element_clear(ciphertext->DTWO[i]);
	}
	
	/* Release the attribute list if one has been allocated. */
	fenc_attribute_list_clear(&(ciphertext->attribute_list));

	memset(ciphertext, 0, sizeof(fenc_ciphertext_WatersCP));
	
	return FENC_ERROR_NONE;
}

/*!
 * Initialize and allocate a fenc_global_params_WatersCP structure.
 *
 * @param	group_params		A fenc_group_params structure.
 * @param	global_params		An allocated fenc_global_params_WatersCP or NULL if one should be allocated.
 * @return	An allocated fenc_global_params_WatersCP structure.
 */

fenc_global_params_WatersCP*
initialize_global_params_WatersCP(fenc_group_params *group_params, fenc_global_params_WatersCP *global_params)
{
	FENC_ERROR err_code;
	
	/* If we need to, allocate a new set of global params for the LSW scheme.	*/
	if (global_params == NULL) {	
		global_params = SAFE_MALLOC(sizeof(fenc_global_params_WatersCP));
		if (global_params == NULL) {
			LOG_ERROR("%s: out of memory", __func__);
			return NULL;
		}
	}
	
	err_code = libfenc_copy_group_params(group_params, &(global_params->group_params));
	if (err_code != FENC_ERROR_NONE) {
		LOG_ERROR("%s: could not copy parameters", __func__);
		return NULL;
	}
	
	err_code = libfenc_get_pbc_pairing(group_params, global_params->pairing);
	if (err_code != FENC_ERROR_NONE) {
		LOG_ERROR("%s: could not obtain pairing structure", __func__);
		return NULL;
	}
	
	return global_params;
}

/*!
 * Allocates and initializes a fenc_key_WatersCP structure.
 *
 * @param key_WatersCP			The fenc_key_WatersCP structure.
 * @param attribute_list	Pointer to a fenc_attribute_list structure.
 * @param policy			Pointer to a fenc_policy structure (the internals are /not/ duplicated).
 * @param copy_attr_list	If set to TRUE, duplicates the internals of the attribute list (original can be cleared).
 * @param global_params		Pointer to the group params (necessary for allocating internal elements).
 * @return					The fenc_key_WatersCP structure or NULL.
 */

fenc_key_WatersCP*
fenc_key_WatersCP_initialize(fenc_attribute_list *attribute_list, Bool copy_attr_list, 
				   fenc_global_params_WatersCP *global_params)
{
	FENC_ERROR err_code;
	size_t i;
	fenc_key_WatersCP *key_WatersCP;
				
	/* Initialize and wipe the key structure.	*/
	key_WatersCP = (fenc_key_WatersCP*)SAFE_MALLOC(sizeof(fenc_key_WatersCP));
	if (key_WatersCP == NULL) {
		LOG_ERROR("fenc_key_WatersCP_initialize: out of memory");
		return NULL;
	}
	memset(key_WatersCP, 0, sizeof(fenc_key_WatersCP));
	key_WatersCP->reference_count = 1;
	
	/* Copy the attribute list structure into the key.  If copy_attr_list is TRUE we
	 * call fenc_attribute_list_copy() to duplicate all of the internals.  Otherwise
	 * we just copy the top-level structure.	*/
	if (copy_attr_list == FALSE) {
		memcpy(&(key_WatersCP->attribute_list), attribute_list, sizeof(fenc_attribute_list));
		key_WatersCP->attribute_list.num_attributes = attribute_list->num_attributes;
	} else {
		err_code = fenc_attribute_list_copy(&(key_WatersCP->attribute_list), attribute_list, global_params->pairing);
		if (err_code != FENC_ERROR_NONE) {
			return NULL;
		}
	}
	
	/* Allocate the internal group elements.	*/
	element_init_G2(key_WatersCP->KTWO, global_params->pairing);
	element_init_G2(key_WatersCP->LTWO, global_params->pairing);

	key_WatersCP->num_components = attribute_list->num_attributes;
	for (i = 0; i < key_WatersCP->attribute_list.num_attributes; i++) {
		element_init_G1(key_WatersCP->KXONE[i], global_params->pairing);
	}
	
	return key_WatersCP;
}


/*!
 * Deallocate and clear the internals of a fenc_key_WatersCP structure.
 *
 * @param key_WatersCP			The fenc_key_WatersCP structure.
 * @return					FENC_ERROR_NONE or an error code.
 */

FENC_ERROR
fenc_key_WatersCP_clear(fenc_key_WatersCP *key_WatersCP)
{	
	size_t i;
	
	element_clear(key_WatersCP->KTWO);
	element_clear(key_WatersCP->LTWO);
	
	for (i = 0; i < key_WatersCP->attribute_list.num_attributes; i++) {
		element_clear(key_WatersCP->KXONE[i]);
	}
	
	if (key_WatersCP->reference_count <= 1) {
		SAFE_FREE(key_WatersCP);
	} else {
		key_WatersCP->reference_count--;
	}
	
	return FENC_ERROR_NONE;
}

/*!
 * Initialize a fenc_public_params_WatersCP structure.  This requires initializing
 * a series of group element structures.
 *
 * @param params			Pointer to a fenc_public_params_WatersCP data structure.
 * @param pairing			Pairing structure.
 * @return					FENC_ERROR_NONE or an error code.
 */

FENC_ERROR
public_params_initialize_WatersCP(fenc_public_params_WatersCP *params, pairing_t pairing)
{
	memset(params, 0, sizeof(fenc_public_params_WatersCP));
	
	element_init_G1(params->gONE, pairing);
	element_init_G2(params->gTWO, pairing);
	element_init_G1(params->gaONE, pairing);
	element_init_G2(params->gaTWO, pairing);
	element_init_GT(params->eggalphaT, pairing);
	
	return FENC_ERROR_NONE;
}

/*!
 * Initialize a fenc_secret_params_WatersCP structure.  This requires initializing
 * a series of group element structures.
 *
 * @param params			Pointer to a fenc_secret_params_WatersCP data structure.
 * @param pairing			Pairing structure.
 * @return					FENC_ERROR_NONE or an error code.
 */

FENC_ERROR
secret_params_initialize_WatersCP(fenc_secret_params_WatersCP *params, pairing_t pairing)
{
	memset(params, 0, sizeof(fenc_secret_params_WatersCP));

	element_init_Zr(params->alphaZ, pairing);
	
	return FENC_ERROR_NONE;
}

/*!
 * Print a ciphertext to a file as ASCII.
 *
 * @param ciphertext		The ciphertext to serialize.
 * @param out_file			The file to write to.
 * @return					FENC_ERROR_NONE or an error code.
 */

FENC_ERROR
libfenc_fprint_ciphertext_WatersCP(fenc_ciphertext_WatersCP *ciphertext, FILE* out_file)
{
	size_t i;
	
	fprintf(out_file, "number of attributes = %d\n", ciphertext->attribute_list.num_attributes);

	fprintf(out_file, "policy = %s\n", ciphertext->policy_str);
	element_fprintf(out_file, "CT = %B\n", ciphertext->CT);
	element_fprintf(out_file, "CprimeONE = %B\n", ciphertext->CprimeONE);
	
	/* For every attribute in the ciphertext... */
	for (i = 0; i < ciphertext->attribute_list.num_attributes; i++) {
		fprintf(out_file, "Attribute #%zu:\n", i);
		if (strlen((char *) ciphertext->attribute_list.attribute[i].attribute_str) > 0) {
			fprintf(out_file, "\tAttribute = \"%s\"\n", ciphertext->attribute_list.attribute[i].attribute_str);
		}
		element_fprintf(out_file, "\tAttribute Hash = %B\n", ciphertext->attribute_list.attribute[i].attribute_hash);
		
		element_fprintf(out_file, "\tCONE[%d] = %B\n", i, ciphertext->CONE[i]);
		element_fprintf(out_file, "\tDTWO[%d] = %B\n", i, ciphertext->DTWO[i]);
	}
	
	/* Return success. */
	return FENC_ERROR_NONE;
}
