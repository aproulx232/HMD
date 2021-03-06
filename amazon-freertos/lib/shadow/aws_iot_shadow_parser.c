/*
 * Copyright (C) 2018 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/**
 * @file aws_iot_shadow_parser.c
 * @brief Implements topic name and JSON parsing functions of the Shadow library.
 */

/* Build using a config header, if provided. */
#ifdef AWS_IOT_CONFIG_FILE
    #include AWS_IOT_CONFIG_FILE
#endif

/* Standard includes. */
#include <stdlib.h>
#include <string.h>

/* Shadow internal include. */
#include "private/aws_iot_shadow_internal.h"

/* JSON utilities include. */
#include "aws_iot_json_utils.h"

/*-----------------------------------------------------------*/

/**
 * @brief The JSON key for the error code in a Shadow error document.
 */
#define _ERROR_DOCUMENT_CODE_KEY              "code"

/**
 * @brief The length of #_ERROR_DOCUMENT_CODE_KEY.
 */
#define _ERROR_DOCUMENT_CODE_KEY_LENGTH       ( sizeof( _ERROR_DOCUMENT_CODE_KEY ) - 1 )

/**
 * @brief The JSON key for the error message in a Shadow error document.
 */
#define _ERROR_DOCUMENT_MESSAGE_KEY           "message"

/**
 * @brief The length of #_ERROR_DOCUMENT_MESSAGE_KEY.
 */
#define _ERROR_DOCUMENT_MESSAGE_KEY_LENGTH    ( sizeof( _ERROR_DOCUMENT_MESSAGE_KEY ) - 1 )

/**
 * @brief The minimum possible length of a Shadow topic name, per the Shadow
 * spec.
 */
#define _MINIMUM_SHADOW_TOPIC_NAME_LENGTH                                 \
    ( _SHADOW_TOPIC_PREFIX_LENGTH +                                       \
      ( uint16_t ) sizeof( _SHADOW_GET_OPERATION_STRING ) +               \
      ( _SHADOW_ACCEPTED_SUFFIX_LENGTH < _SHADOW_REJECTED_SUFFIX_LENGTH ? \
        _SHADOW_ACCEPTED_SUFFIX_LENGTH :                                  \
        _SHADOW_REJECTED_SUFFIX_LENGTH ) )

/*-----------------------------------------------------------*/

/**
 * @brief Converts a `unsigned long` to an `AwsIotShadowError_t`.
 *
 * @param[in] code A value between 400 and 500 to convert.
 *
 * @return A corresponding #AwsIotShadowError_t; #AWS_IOT_SHADOW_BAD_RESPONSE
 * if `code` is unknown.
 */
static AwsIotShadowError_t _codeToShadowStatus( unsigned long code );

/*-----------------------------------------------------------*/

static AwsIotShadowError_t _codeToShadowStatus( unsigned long code )
{
    /* Convert the Shadow response code to an AwsIotShadowError_t. */
    switch( code )
    {
        case 400UL:

            return AWS_IOT_SHADOW_BAD_REQUEST;

        case 401UL:

            return AWS_IOT_SHADOW_UNAUTHORIZED;

        case 403UL:

            return AWS_IOT_SHADOW_FORBIDDEN;

        case 404UL:

            return AWS_IOT_SHADOW_NOT_FOUND;

        case 409UL:

            return AWS_IOT_SHADOW_CONFLICT;

        case 413UL:

            return AWS_IOT_SHADOW_TOO_LARGE;

        case 415UL:

            return AWS_IOT_SHADOW_UNSUPPORTED;

        case 429UL:

            return AWS_IOT_SHADOW_TOO_MANY_REQUESTS;

        case 500UL:

            return AWS_IOT_SHADOW_SERVER_ERROR;

        default:

            return AWS_IOT_SHADOW_BAD_RESPONSE;
    }
}

/*-----------------------------------------------------------*/

_shadowOperationStatus_t AwsIotShadowInternal_ParseShadowStatus( const char * const pTopicName,
                                                                 size_t topicNameLength )
{
    const char * pSuffixStart = NULL;

    /* Check that the Shadow status topic name is at least as long as the
     * "accepted" suffix. */
    if( topicNameLength > _SHADOW_ACCEPTED_SUFFIX_LENGTH )
    {
        /* Calculate where the "accepted" suffix should start. */
        pSuffixStart = pTopicName + topicNameLength - _SHADOW_ACCEPTED_SUFFIX_LENGTH;

        /* pSuffixStart must be in pTopicName. */
        AwsIotShadow_Assert( ( pSuffixStart > pTopicName ) &&
                             ( pSuffixStart < pTopicName + topicNameLength ) );

        /* Check if the end of the Shadow status topic name is "accepted". */
        if( strncmp( pSuffixStart,
                     _SHADOW_ACCEPTED_SUFFIX,
                     _SHADOW_ACCEPTED_SUFFIX_LENGTH ) == 0 )
        {
            return _SHADOW_ACCEPTED;
        }
    }

    /* Check that the Shadow status topic name is at least as long as the
     * "rejected" suffix. */
    if( topicNameLength > _SHADOW_REJECTED_SUFFIX_LENGTH )
    {
        /* Calculate where the "rejected" suffix should start. */
        pSuffixStart = pTopicName + topicNameLength - _SHADOW_REJECTED_SUFFIX_LENGTH;

        /* pSuffixStart must be in pTopicName. */
        AwsIotShadow_Assert( ( pSuffixStart > pTopicName ) &&
                             ( pSuffixStart < pTopicName + topicNameLength ) );

        /* Check if the end of the Shadow status topic name is "rejected". */
        if( strncmp( pSuffixStart,
                     _SHADOW_REJECTED_SUFFIX,
                     _SHADOW_REJECTED_SUFFIX_LENGTH ) == 0 )
        {
            return _SHADOW_REJECTED;
        }
    }

    /* The topic name matched neither "accepted" nor "rejected". */
    return _UNKNOWN_STATUS;
}

/*-----------------------------------------------------------*/

AwsIotShadowError_t AwsIotShadowInternal_ParseErrorDocument( const char * const pErrorDocument,
                                                             size_t errorDocumentLength )
{
    const char * pCode = NULL, * pMessage = NULL;
    size_t codeLength = 0, messageLength = 0;
    unsigned long code = 0;

    /* Parse the code from the error document. */
    if( AwsIotJsonUtils_FindJsonValue( pErrorDocument,
                                       errorDocumentLength,
                                       _ERROR_DOCUMENT_CODE_KEY,
                                       _ERROR_DOCUMENT_CODE_KEY_LENGTH,
                                       &pCode,
                                       &codeLength ) == false )
    {
        /* Error parsing JSON document, or no "code" key was found. */
        AwsIotLogWarn( "Failed to parse code from error document.\n%.*s",
                       errorDocumentLength,
                       pErrorDocument );

        return AWS_IOT_SHADOW_BAD_RESPONSE;
    }

    /* Code must be in error document. */
    AwsIotShadow_Assert( ( pCode > pErrorDocument ) &&
                         ( pCode + codeLength < pErrorDocument + errorDocumentLength ) );

    /* Convert the code to an unsigned integer value. */
    code = strtoul( pCode, NULL, 10 );

    /* Parse the error message and print it. An error document must always contain
     * a message. */
    if( AwsIotJsonUtils_FindJsonValue( pErrorDocument,
                                       errorDocumentLength,
                                       _ERROR_DOCUMENT_MESSAGE_KEY,
                                       _ERROR_DOCUMENT_MESSAGE_KEY_LENGTH,
                                       &pMessage,
                                       &messageLength ) == true )
    {
        AwsIotLogWarn( "Code %lu: %.*s.",
                       code,
                       messageLength,
                       pMessage );
    }
    else
    {
        AwsIotLogWarn( "Code %lu; failed to parse message from error document.\n%.*s",
                       code,
                       errorDocumentLength,
                       pErrorDocument );

        /* An error document must contain a message; if it does not, then it is invalid. */
        return AWS_IOT_SHADOW_BAD_RESPONSE;
    }

    return _codeToShadowStatus( code );
}

/*-----------------------------------------------------------*/

AwsIotShadowError_t AwsIotShadowInternal_ParseThingName( const char * const pTopicName,
                                                         uint16_t topicNameLength,
                                                         const char ** const pThingName,
                                                         size_t * const pThingNameLength )
{
    const char * pThingNameStart = NULL;
    size_t thingNameLength = 0;

    /* Check that the topic name length exceeds the minimum possible length. */
    if( topicNameLength < _MINIMUM_SHADOW_TOPIC_NAME_LENGTH )
    {
        return AWS_IOT_SHADOW_BAD_RESPONSE;
    }

    /* All Shadow topic names must start with the same prefix. */
    if( strncmp( _SHADOW_TOPIC_PREFIX,
                 pTopicName,
                 _SHADOW_TOPIC_PREFIX_LENGTH ) != 0 )
    {
        return AWS_IOT_SHADOW_BAD_RESPONSE;
    }

    /* The Thing Name starts immediately after the topic prefix. */
    pThingNameStart = pTopicName + _SHADOW_TOPIC_PREFIX_LENGTH;

    /* Calculate the length of the Thing Name. */
    while( ( thingNameLength + _SHADOW_TOPIC_PREFIX_LENGTH < ( size_t ) topicNameLength ) &&
           ( pThingNameStart[ thingNameLength ] != '/' ) )
    {
        thingNameLength++;
    }

    /* The end of the topic name was reached without finding a '/'. The topic
     * name is invalid. */
    if( thingNameLength + _SHADOW_TOPIC_PREFIX_LENGTH >= ( size_t ) topicNameLength )
    {
        return AWS_IOT_SHADOW_BAD_RESPONSE;
    }

    /* Set the output parameters. */
    *pThingName = pThingNameStart;
    *pThingNameLength = thingNameLength;

    return AWS_IOT_SHADOW_SUCCESS;
}

/*-----------------------------------------------------------*/
