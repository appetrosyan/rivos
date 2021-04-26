/**
 * @file x509_cert_validate.c
 * @brief X.509 certificate validation
 *
 * @section License
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Copyright (C) 2010-2021 Oryx Embedded SARL. All rights reserved.
 *
 * This file is part of CycloneCRYPTO Open.
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * @author Oryx Embedded SARL (www.oryx-embedded.com)
 * @version 2.0.4
 **/

//Switch to the appropriate trace level
#define TRACE_LEVEL CRYPTO_TRACE_LEVEL

//Dependencies
#include "core/crypto.h"
#include "pkix/x509_cert_parse.h"
#include "pkix/x509_cert_validate.h"
#include "pkix/x509_key_parse.h"
#include "encoding/asn1.h"
#include "encoding/oid.h"
#include "pkc/rsa.h"
#include "pkc/dsa.h"
#include "ecc/ecdsa.h"
#include "ecc/ed25519.h"
#include "ecc/ed448.h"
#include "debug.h"

//Check crypto library configuration
#if (X509_SUPPORT == ENABLED)


/**
 * @brief X.509 certificate validation
 * @param[in] certInfo X.509 certificate to be verified
 * @param[in] issuerCertInfo Issuer certificate
 * @param[in] pathLen Certificate path length
 * @return Error code
 **/

error_t x509ValidateCertificate(const X509CertificateInfo *certInfo,
   const X509CertificateInfo *issuerCertInfo, uint_t pathLen)
{
   error_t error;
   time_t currentTime;
   const X509Extensions *extensions;

   //Check parameters
   if(certInfo == NULL || issuerCertInfo == NULL)
      return ERROR_INVALID_PARAMETER;

   //Retrieve current time
   currentTime = getCurrentUnixTime();

   //Any real-time clock implemented?
   if(currentTime != 0)
   {
      DateTime currentDate;
      const X509Validity *validity;

      //Convert Unix timestamp to date
      convertUnixTimeToDate(currentTime, &currentDate);

      //The certificate validity period is the time interval during which the
      //CA warrants that it will maintain information about the status of the
      //certificate
      validity = &certInfo->tbsCert.validity;

      //Check the validity period
      if(compareDateTime(&currentDate, &validity->notBefore) < 0 ||
         compareDateTime(&currentDate, &validity->notAfter) > 0)
      {
         //The certificate has expired or is not yet valid
         return ERROR_CERTIFICATE_EXPIRED;
      }
   }

   //Make sure that the subject and issuer names chain correctly
   if(!x509CompareName(certInfo->tbsCert.issuer.rawData,
      certInfo->tbsCert.issuer.rawDataLen,
      issuerCertInfo->tbsCert.subject.rawData,
      issuerCertInfo->tbsCert.subject.rawDataLen))
   {
      //Report an error
      return ERROR_BAD_CERTIFICATE;
   }

   //Point to the X.509 extensions of the issuer certificate
   extensions = &issuerCertInfo->tbsCert.extensions;

   //X.509 version 3 certificate?
   if(issuerCertInfo->tbsCert.version >= X509_VERSION_3)
   {
      //Ensure that the issuer certificate is a CA certificate
      if(!extensions->basicConstraints.cA)
         return ERROR_BAD_CERTIFICATE;
   }

   //Where pathLenConstraint does not appear, no limit is imposed
   if(extensions->basicConstraints.pathLenConstraint >= 0)
   {
      //The pathLenConstraint field gives the maximum number of non-self-issued
      //intermediate certificates that may follow this certificate in a valid
      //certification path
      if(pathLen > (uint_t) extensions->basicConstraints.pathLenConstraint)
         return ERROR_BAD_CERTIFICATE;
   }

   //Check if the keyUsage extension is present
   if(extensions->keyUsage.bitmap != 0)
   {
      //If the keyUsage extension is present, then the subject public key must
      //not be used to verify signatures on certificates unless the keyCertSign
      //bit is set (refer to RFC 5280, section 4.2.1.3)
      if((extensions->keyUsage.bitmap & X509_KEY_USAGE_KEY_CERT_SIGN) == 0)
         return ERROR_BAD_CERTIFICATE;
   }

   //The ASN.1 DER-encoded tbsCertificate is used as the input to the signature
   //function
   error = x509VerifySignature(certInfo->tbsCert.rawData,
      certInfo->tbsCert.rawDataLen, &certInfo->signatureAlgo,
      &issuerCertInfo->tbsCert.subjectPublicKeyInfo, &certInfo->signatureValue);

   //Return status code
   return error;
}


/**
 * @brief Check whether the certificate matches the specified FQDN
 * @param[in] certInfo Pointer to the X.509 certificate
 * @param[in] fqdn NULL-terminated string that contains the fully-qualified domain name
 * @return Error code
 **/

error_t x509CheckSubjectName(const X509CertificateInfo *certInfo,
   const char_t *fqdn)
{
   error_t error;
   bool_t res;
   uint_t i;
   size_t n;
   size_t length;
   const uint8_t *data;
   const X509Extensions *extensions;
   X509GeneralName generalName;

   //Point to the X.509 extensions of the CA certificate
   extensions = &certInfo->tbsCert.extensions;

   //Valid FQDN name provided?
   if(fqdn != NULL)
   {
      //Initialize flag
      res = FALSE;

      //Total number of valid DNS names found in the SubjectAltName extension
      i = 0;

      //Valid SubjectAltName extension?
      if(extensions->subjectAltName.rawDataLen > 0)
      {
         //The subject alternative name extension allows identities to be bound
         //to the subject of the certificate. These identities may be included
         //in addition to or in place of the identity in the subject field of
         //the certificate
         data = extensions->subjectAltName.rawData;
         length = extensions->subjectAltName.rawDataLen;

         //Loop through the list of subject alternative names
         while(!res && length > 0)
         {
            //Parse GeneralName field
            error = x509ParseGeneralName(data, length, &n, &generalName);
            //Failed to decode ASN.1 tag?
            if(error)
               return error;

            //DNS name found?
            if(generalName.type == X509_GENERAL_NAME_TYPE_DNS)
            {
               //Check whether the alternative name matches the specified FQDN
               res = x509CompareSubjectName(generalName.value,
                  generalName.length, fqdn);

               //Increment counter
               i++;
            }

            //Next item
            data += n;
            length -= n;
         }
      }

      //No match?
      if(!res)
      {
         //The implementation must not seek a match for a reference identifier
         //of CN-ID if the presented identifiers include a DNS-ID, SRV-ID or
         //URI-ID (refer to RFC 6125, section 6.4.4)
         if(i == 0 && certInfo->tbsCert.subject.commonNameLen > 0)
         {
            //The implementation may as a last resort check the CN-ID for a match
            res = x509CompareSubjectName(certInfo->tbsCert.subject.commonName,
               certInfo->tbsCert.subject.commonNameLen, fqdn);
         }
      }

      //Check whether the subject name matches the specified FQDN
      error = res ? NO_ERROR : ERROR_INVALID_NAME;
   }
   else
   {
      //If no valid FQDN name is provided, then the subject name of the
      //certificate is not verified
      error = NO_ERROR;
   }

   //Return status code
   return error;
}


/**
 * @brief Check name constraints
 * @param[in] subjectName Subject name to be verified
 * @param[in] certInfo Pointer to the CA certificate
 * @return Error code
 **/

error_t x509CheckNameConstraints(const char_t *subjectName,
   const X509CertificateInfo *certInfo)
{
   error_t error;
   bool_t match;
   size_t m;
   size_t n;
   size_t length;
   const uint8_t *data;
   const X509Extensions *extensions;
   X509GeneralName subtree;

   //Initialize error code
   error = NO_ERROR;

   //Point to the X.509 extensions of the CA certificate
   extensions = &certInfo->tbsCert.extensions;

   //Valid subject name provided?
   if(subjectName != NULL)
   {
      //Point to the list of excluded name subtrees
      data = extensions->nameConstraints.excludedSubtrees;
      length = extensions->nameConstraints.excludedSubtreesLen;

      //Loop through the names constraints
      while(length > 0)
      {
         //Parse GeneralSubtree field
         error = x509ParseGeneralSubtree(data, length, &n, &subtree);
         //Failed to decode ASN.1 tag?
         if(error)
            break;

         //Initialize flag
         match = FALSE;

         //Check name type
         if(subtree.type == X509_GENERAL_NAME_TYPE_DNS)
         {
            //Check whether the subject name matches the subtree
            match = x509CompareSubtree(subjectName, subtree.value,
               subtree.length);
         }
         else if(subtree.type == X509_GENERAL_NAME_TYPE_DIRECTORY)
         {
            X509Name name;

            //Parse distinguished name
            error = x509ParseName((uint8_t *) subtree.value, subtree.length,
               &m, &name);
            //Failed to decode ASN.1 structure?
            if(error)
               break;

            //Valid common name?
            if(name.commonName != NULL)
            {
               //Check whether the subject name matches the subtree
               match = x509CompareSubtree(subjectName, name.commonName,
                  name.commonNameLen);
            }
         }
         else
         {
            //Just for sanity
         }

         //Any match?
         if(match)
         {
            //The subject name is not acceptable
            error = ERROR_INVALID_NAME;
            break;
         }

         //Next item
         data += n;
         length -= n;
      }

      //Any name matching a restriction in the excludedSubtrees field is
      //invalid regardless of information appearing in the permittedSubtrees
      //(Refer to RFC 5280, section 4.2.1.10)
      if(!error)
      {
         //Point to the list of permitted name subtrees
         data = extensions->nameConstraints.permittedSubtrees;
         length = extensions->nameConstraints.permittedSubtreesLen;

         //Loop through the names constraints
         while(length > 0)
         {
            //Parse GeneralSubtree field
            error = x509ParseGeneralSubtree(data, length, &n, &subtree);
            //Failed to decode ASN.1 tag?
            if(error)
               break;

            //Initialize flag
            match = FALSE;

            //Check name type
            if(subtree.type == X509_GENERAL_NAME_TYPE_DNS)
            {
               //Check whether the subject name matches the subtree
               match = x509CompareSubtree(subjectName, subtree.value,
                  subtree.length);
            }
            else if(subtree.type == X509_GENERAL_NAME_TYPE_DIRECTORY)
            {
               X509Name name;

               //Parse distinguished name
               error = x509ParseName((uint8_t *) subtree.value, subtree.length,
                  &m, &name);
               //Failed to decode ASN.1 structure?
               if(error)
                  break;

               //Valid common name?
               if(name.commonName != NULL)
               {
                  //Check whether the subject name matches the subtree
                  match = x509CompareSubtree(subjectName, name.commonName,
                     name.commonNameLen);
               }
            }
            else
            {
               //Just for sanity
            }

            //Any match?
            if(match)
            {
               //The subject name is acceptable
               error = NO_ERROR;
               break;
            }
            else
            {
               //The subject name does not match the current field
               error = ERROR_INVALID_NAME;
            }

            //Next item
            data += n;
            length -= n;
         }
      }
   }
   else
   {
      //If no valid subject name is provided, then the name constraints
      //are not verified
   }

   //Return status code
   return error;
}


/**
 * @brief Compare distinguished names
 * @param[in] name1 Pointer to the first distinguished name
 * @param[in] nameLen1 Length of the first distinguished name
 * @param[in] name2 Pointer to the second distinguished name
 * @param[in] nameLen2 Length of the second distinguished name
 * @return Comparison result
 **/

bool_t x509CompareName(const uint8_t *name1, size_t nameLen1,
   const uint8_t *name2, size_t nameLen2)
{
   //Compare the length of the distinguished names
   if(nameLen1 != nameLen2)
      return FALSE;

   //Compare the contents of the distinguished names
   if(osMemcmp(name1, name2, nameLen1))
      return FALSE;

   //The distinguished names match
   return TRUE;
}


/**
 * @brief Check whether the subject name matches the specified FQDN
 * @param[in] subjectName Subject name
 * @param[in] subjectNameLen Length of the subject name
 * @param[in] fqdn NULL-terminated string that contains the fully-qualified domain name
 * @return TRUE if the subject name matches the specified FQDN, else FALSE
 **/

bool_t x509CompareSubjectName(const char_t *subjectName,
   size_t subjectNameLen, const char_t *fqdn)
{
   size_t i;
   size_t j;
   size_t fqdnLen;

   //Retrieve the length of the FQDN
   fqdnLen = osStrlen(fqdn);

   //Initialize variables
   i = 0;
   j = 0;

   //Parse the subject name
   while(i < subjectNameLen && j < fqdnLen)
   {
      //Wildcard name found?
      if(subjectName[i] == '*')
      {
         //The implementation should not attempt to match a presented
         //identifier in which the wildcard character comprises a label other
         //than the left-most label (refer to RFC 6125, section 6.4.3)
         if(i != 0)
         {
            break;
         }

         //The implementation should not compare against anything but the
         //left-most label of the reference identifier
         if(fqdn[j] == '.')
         {
            i++;
         }
         else
         {
            j++;
         }
      }
      else
      {
         //Perform case insensitive character comparison
         if(osTolower(subjectName[i]) != fqdn[j])
         {
            break;
         }

         //Compare next characters
         i++;
         j++;
      }
   }

   //Check whether the subject name matches the specified FQDN
   if(i == subjectNameLen && j == fqdnLen)
   {
      return TRUE;
   }
   else
   {
      return FALSE;
   }
}


/**
 * @brief Compare a subject name against the specified subtree
 * @param[in] subjectName NULL-terminated string that contains the subject name
 * @param[in] subtree Pointer to the subtree
 * @param[in] subtreeLen Length of the subtree
 * @return Comparison result
 **/

bool_t x509CompareSubtree(const char_t *subjectName,
   const char_t *subtree, size_t subtreeLen)
{
   int_t i;
   int_t j;

   //Point to the last character of the subtree
   i = subtreeLen - 1;
   //Point to the last character of the subject name
   j = osStrlen(subjectName) - 1;

   //Parse the subtree
   while(i >= 0 && j >= 0)
   {
      //Perform case insensitive character comparison
      if(osTolower(subtree[i]) != subjectName[j])
      {
         break;
      }

      //The constraint may specify a host or a domain
      if(subtree[i] == '.' && i == 0)
      {
         //When the constraint begins with a period, it may be expanded with
         //one or more labels (refer to RFC 5280, section 4.2.1.10)
         i = -1;
         j = -1;
      }
      else
      {
         //Compare previous characters
         i--;
         j--;
      }
   }

   //Check whether the subject name matches the specified subtree
   if(i < 0 && j < 0)
   {
      return TRUE;
   }
   else
   {
      return FALSE;
   }
}


/**
 * @brief Certificate signature verification
 * @param[in] tbsCert TBSCertificate whose signature is to be verified
 * @param[in] tbsCertLen Length of the TBSCertificate, in bytes
 * @param[in] signatureAlgoId Signature algorithm identifier
 * @param[in] publicKeyInfo Issuer's public key
 * @param[in] signatureValue Signature to be verified
 * @return Error code
 **/

error_t x509VerifySignature(const uint8_t *tbsCert, size_t tbsCertLen,
   const X509SignatureAlgoId *signatureAlgoId,
   const X509SubjectPublicKeyInfo *publicKeyInfo,
   const X509SignatureValue *signatureValue)
{
   error_t error;
   X509SignatureAlgo signAlgo;
   const HashAlgo *hashAlgo;
   uint8_t digest[X509_MAX_HASH_DIGEST_SIZE];

   //Retrieve the signature algorithm that was used to sign the certificate
   error = x509GetSignHashAlgo(signatureAlgoId, &signAlgo, &hashAlgo);
   //Unsupported signature algorithm?
   if(error)
      return error;

   //Ed25519 and Ed448 are used in PureEdDSA mode, without pre-hashing
   if(hashAlgo != NULL)
   {
      //Digest the TBSCertificate structure using the specified hash algorithm
      error = hashAlgo->compute(tbsCert, tbsCertLen, digest);
      //Any error to report?
      if(error)
         return error;

#if (X509_RSA_SUPPORT == ENABLED && RSA_SUPPORT == ENABLED)
      //RSA signature algorithm?
      if(signAlgo == X509_SIGN_ALGO_RSA)
      {
         uint_t k;
         RsaPublicKey publicKey;

         //Initialize RSA public key
         rsaInitPublicKey(&publicKey);

         //Import the RSA public key
         error = x509ImportRsaPublicKey(publicKeyInfo, &publicKey);

         //Check status code
         if(!error)
         {
            //Get the length of the modulus, in bits
            k = mpiGetBitLength(&publicKey.n);

            //Make sure the modulus is acceptable
            if(k < X509_MIN_RSA_MODULUS_SIZE || k > X509_MAX_RSA_MODULUS_SIZE)
            {
               //Report an error
               error = ERROR_INVALID_KEY;
            }
         }

         //Check status code
         if(!error)
         {
            //Verify RSA signature (RSASSA-PKCS1-v1_5 signature scheme)
            error = rsassaPkcs1v15Verify(&publicKey, hashAlgo, digest,
               signatureValue->data, signatureValue->length);
         }

         //Release previously allocated resources
         rsaFreePublicKey(&publicKey);
      }
      else
#endif
#if (X509_RSA_PSS_SUPPORT == ENABLED && RSA_SUPPORT == ENABLED)
      //RSA-PSS signature algorithm?
      if(signAlgo == X509_SIGN_ALGO_RSA_PSS)
      {
         uint_t k;
         RsaPublicKey publicKey;

         //Initialize RSA public key
         rsaInitPublicKey(&publicKey);

         //Import the RSA public key
         error = x509ImportRsaPublicKey(publicKeyInfo, &publicKey);

         //Check status code
         if(!error)
         {
            //Get the length of the modulus, in bits
            k = mpiGetBitLength(&publicKey.n);

            //Make sure the modulus is acceptable
            if(k < X509_MIN_RSA_MODULUS_SIZE || k > X509_MAX_RSA_MODULUS_SIZE)
            {
               //Report an error
               error = ERROR_INVALID_KEY;
            }
         }

         //Check status code
         if(!error)
         {
            //Retrieve the length of the salt
            k = signatureAlgoId->rsaPssParams.saltLen;

            //Verify RSA signature (RSASSA-PSS signature scheme)
            error = rsassaPssVerify(&publicKey, hashAlgo, k, digest,
               signatureValue->data, signatureValue->length);
         }

         //Release previously allocated resources
         rsaFreePublicKey(&publicKey);
      }
      else
#endif
#if (X509_DSA_SUPPORT == ENABLED && DSA_SUPPORT == ENABLED)
      //DSA signature algorithm?
      if(signAlgo == X509_SIGN_ALGO_DSA)
      {
         uint_t k;
         DsaPublicKey publicKey;
         DsaSignature signature;

         //Initialize DSA public key
         dsaInitPublicKey(&publicKey);
         //Initialize DSA signature
         dsaInitSignature(&signature);

         //Import the DSA public key
         error = x509ImportDsaPublicKey(publicKeyInfo, &publicKey);

         //Check status code
         if(!error)
         {
            //Get the length of the prime modulus, in bits
            k = mpiGetBitLength(&publicKey.p);

            //Make sure the prime modulus is acceptable
            if(k < X509_MIN_DSA_MODULUS_SIZE || k > X509_MAX_DSA_MODULUS_SIZE)
            {
               //Report an error
               error = ERROR_INVALID_KEY;
            }
         }

         //Check status code
         if(!error)
         {
            //Read the ASN.1 encoded signature
            error = dsaReadSignature(signatureValue->data,
               signatureValue->length, &signature);
         }

         //Check status code
         if(!error)
         {
            //Verify DSA signature
            error = dsaVerifySignature(&publicKey, digest, hashAlgo->digestSize,
               &signature);
         }

         //Release previously allocated resources
         dsaFreePublicKey(&publicKey);
         dsaFreeSignature(&signature);
      }
      else
#endif
#if (X509_ECDSA_SUPPORT == ENABLED && ECDSA_SUPPORT == ENABLED)
      //ECDSA signature algorithm?
      if(signAlgo == X509_SIGN_ALGO_ECDSA)
      {
         const EcCurveInfo *curveInfo;
         EcDomainParameters params;
         EcPoint publicKey;
         EcdsaSignature signature;

         //Initialize EC domain parameters
         ecInitDomainParameters(&params);
         //Initialize ECDSA public key
         ecInit(&publicKey);
         //Initialize ECDSA signature
         ecdsaInitSignature(&signature);

         //Retrieve EC domain parameters
         curveInfo = x509GetCurveInfo(publicKeyInfo->ecParams.namedCurve,
            publicKeyInfo->ecParams.namedCurveLen);

         //Make sure the specified elliptic curve is supported
         if(curveInfo != NULL)
         {
            //Load EC domain parameters
            error = ecLoadDomainParameters(&params, curveInfo);
         }
         else
         {
            //Invalid EC domain parameters
            error = ERROR_BAD_CERTIFICATE;
         }

         //Check status code
         if(!error)
         {
            //Retrieve the EC public key
            error = ecImport(&params, &publicKey, publicKeyInfo->ecPublicKey.q,
               publicKeyInfo->ecPublicKey.qLen);
         }

         //Check status code
         if(!error)
         {
            //Read the ASN.1 encoded signature
            error = ecdsaReadSignature(signatureValue->data,
               signatureValue->length, &signature);
         }

         //Check status code
         if(!error)
         {
            //Verify ECDSA signature
            error = ecdsaVerifySignature(&params, &publicKey, digest,
               hashAlgo->digestSize, &signature);
         }

         //Release previously allocated resources
         ecFreeDomainParameters(&params);
         ecFree(&publicKey);
         ecdsaFreeSignature(&signature);
      }
      else
#endif
      //Invalid signature algorithm?
      {
         //Report an error
         error = ERROR_UNSUPPORTED_SIGNATURE_ALGO;
      }
   }
   else
   {
#if (X509_ED25519_SUPPORT == ENABLED && ED25519_SUPPORT == ENABLED)
      //Ed25519 signature algorithm?
      if(signAlgo == X509_SIGN_ALGO_ED25519)
      {
         //Check the length of the public key
         if(publicKeyInfo->ecPublicKey.qLen == ED25519_PUBLIC_KEY_LEN)
         {
            //Check the length of the EdDSA signature
            if(signatureValue->length == ED25519_SIGNATURE_LEN)
            {
               //Verify signature (PureEdDSA mode)
               error = ed25519VerifySignature(publicKeyInfo->ecPublicKey.q,
                  tbsCert, tbsCertLen, NULL, 0, 0, signatureValue->data);
            }
            else
            {
               //The length of the EdDSA signature is not valid
               error = ERROR_INVALID_SIGNATURE;
            }
         }
         else
         {
            //The length of the Ed25519 public key is not valid
            error = ERROR_ILLEGAL_PARAMETER;
         }
      }
      else
#endif
#if (X509_ED448_SUPPORT == ENABLED && ED448_SUPPORT == ENABLED)
      //Ed448 signature algorithm?
      if(signAlgo == X509_SIGN_ALGO_ED448)
      {
         //Check the length of the public key
         if(publicKeyInfo->ecPublicKey.qLen == ED448_PUBLIC_KEY_LEN)
         {
            //Check the length of the EdDSA signature
            if(signatureValue->length == ED448_SIGNATURE_LEN)
            {
               //Verify signature (PureEdDSA mode)
               error = ed448VerifySignature(publicKeyInfo->ecPublicKey.q,
                  tbsCert, tbsCertLen, NULL, 0, 0, signatureValue->data);
            }
            else
            {
               //The length of the EdDSA signature is not valid
               error = ERROR_INVALID_SIGNATURE;
            }
         }
         else
         {
            //The length of the Ed448 public key is not valid
            error = ERROR_ILLEGAL_PARAMETER;
         }
      }
      else
#endif
      //Invalid signature algorithm?
      {
         //Report an error
         error = ERROR_UNSUPPORTED_SIGNATURE_ALGO;
      }
   }

   //Return status code
   return error;
}

#endif
