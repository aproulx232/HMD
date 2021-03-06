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
 * @file aws_iot_static_memory_mqtt_posix.c
 * @brief Implementation of MQTT static memory functions in aws_iot_static_memory.h
 * for POSIX systems.
 */

/* Build using a config header, if provided. */
#ifdef AWS_IOT_CONFIG_FILE
    #include AWS_IOT_CONFIG_FILE
#endif

/* This file should only be compiled if dynamic memory allocation is forbidden. */
#if AWS_IOT_STATIC_MEMORY_ONLY == 1

/* Standard includes. */
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* POSIX include. Allow it to be overridden. */
#ifdef POSIX_PTHREAD_HEADER
    #include POSIX_PTHREAD_HEADER
#else
    #include <pthread.h>
#endif

/* Static memory include. */
#include "platform/aws_iot_static_memory.h"

/* MQTT internal include. */
#include "private/aws_iot_mqtt_internal.h"

/*-----------------------------------------------------------*/

/**
 * @cond DOXYGEN_IGNORE
 * Doxygen should ignore this section.
 *
 * Provide default values for undefined configuration constants.
 */
#ifndef AWS_IOT_MQTT_CONNECTIONS
    #define AWS_IOT_MQTT_CONNECTIONS                   ( 1 )
#endif
#ifndef AWS_IOT_MQTT_SUBSCRIPTIONS
    #define AWS_IOT_MQTT_SUBSCRIPTIONS                 ( 8 )
#endif
#ifndef AWS_IOT_MQTT_MAX_IN_PROGRESS_OPERATIONS
    #define AWS_IOT_MQTT_MAX_IN_PROGRESS_OPERATIONS    ( 10 )
#endif
/** @endcond */

/* Validate static memory configuration settings. */
#if AWS_IOT_MQTT_CONNECTIONS <= 0
    #error "AWS_IOT_MQTT_CONNECTIONS cannot be 0 or negative."
#endif
#if AWS_IOT_MQTT_SUBSCRIPTIONS <= 0
    #error "AWS_IOT_MQTT_SUBSCRIPTIONS cannot be 0 or negative."
#endif
#if AWS_IOT_MQTT_MAX_IN_PROGRESS_OPERATIONS <= 0
    #error "AWS_IOT_MQTT_MAX_IN_PROGRESS_OPERATIONS cannot be 0 or negative."
#endif

/**
 * @brief The size of a static memory MQTT subscription.
 *
 * Since the pTopic member of #_mqttSubscription_t is variable-length, the constant
 * #_AWS_IOT_MQTT_SERVER_MAX_TOPIC_LENGTH is used for the length of
 * #_mqttSubscription_t.pTopicFilter.
 */
#define _MQTT_SUBSCRIPTION_SIZE    ( sizeof( _mqttSubscription_t ) + _AWS_IOT_MQTT_SERVER_MAX_TOPIC_LENGTH )

/*-----------------------------------------------------------*/

/* Extern declarations of common static memory functions in aws_iot_static_memory_common_posix.c
 * Because these functions are POSIX-platform-specific, they are not placed in
 * a platform header file. */
extern int AwsIotStaticMemory_FindFree( bool * const pInUse,
                                        int limit );
extern void AwsIotStaticMemory_ReturnInUse( void * ptr,
                                            void * const pPool,
                                            bool * const pInUse,
                                            int limit,
                                            size_t elementSize );

/*-----------------------------------------------------------*/

/*
 * Static memory buffers and flags, allocated and zeroed at compile-time.
 */
static bool _pInUseMqttConnections[ AWS_IOT_MQTT_CONNECTIONS ] = { 0 };                               /**< @brief MQTT connection in-use flags. */
static _mqttConnection_t _pMqttConnections[ AWS_IOT_MQTT_CONNECTIONS ] = { { 0 } };                   /**< @brief MQTT connections. */

static bool _pInUseMqttOperations[ AWS_IOT_MQTT_MAX_IN_PROGRESS_OPERATIONS ] = { 0 };                 /**< @brief MQTT operation in-use flags. */
static _mqttOperation_t _pMqttOperations[ AWS_IOT_MQTT_MAX_IN_PROGRESS_OPERATIONS ] = { 0 };          /**< @brief MQTT operations. */

static bool _pInUseMqttTimerEvents[ AWS_IOT_MQTT_MAX_IN_PROGRESS_OPERATIONS ] = { 0 };                /**< @brief MQTT timer event in-use flags. */
static _mqttTimerEvent_t _pMqttTimerEvents[ AWS_IOT_MQTT_MAX_IN_PROGRESS_OPERATIONS ] = { 0 };        /**< @brief MQTT timer events. */

static bool _pInUseMqttSubscriptions[ AWS_IOT_MQTT_SUBSCRIPTIONS ] = { 0 };                           /**< @brief MQTT subscription in-use flags. */
static char _pMqttSubscriptions[ AWS_IOT_MQTT_SUBSCRIPTIONS ][ _MQTT_SUBSCRIPTION_SIZE ] = { { 0 } }; /**< @brief MQTT subscriptions. */

/*-----------------------------------------------------------*/

void * AwsIot_MallocMqttConnection( size_t size )
{
    int freeIndex = -1;
    void * pNewConnection = NULL;

    /* Check size argument. */
    if( size == sizeof( _mqttConnection_t ) )
    {
        /* Find a free MQTT connection. */
        freeIndex = AwsIotStaticMemory_FindFree( _pInUseMqttConnections,
                                                 AWS_IOT_MQTT_CONNECTIONS );

        if( freeIndex != -1 )
        {
            pNewConnection = &( _pMqttConnections[ freeIndex ] );
        }
    }

    return pNewConnection;
}

/*-----------------------------------------------------------*/

void AwsIot_FreeMqttConnection( void * ptr )
{
    /* Return the in-use MQTT connection. */
    AwsIotStaticMemory_ReturnInUse( ptr,
                                    _pMqttConnections,
                                    _pInUseMqttConnections,
                                    AWS_IOT_MQTT_CONNECTIONS,
                                    sizeof( _mqttConnection_t ) );
}

/*-----------------------------------------------------------*/

void * AwsIot_MallocMqttOperation( size_t size )
{
    int freeIndex = -1;
    void * pNewOperation = NULL;

    /* Check size argument. */
    if( size == sizeof( _mqttOperation_t ) )
    {
        /* Find a free MQTT operation. */
        freeIndex = AwsIotStaticMemory_FindFree( _pInUseMqttOperations,
                                                 AWS_IOT_MQTT_MAX_IN_PROGRESS_OPERATIONS );

        if( freeIndex != -1 )
        {
            pNewOperation = &( _pMqttOperations[ freeIndex ] );
        }
    }

    return pNewOperation;
}

/*-----------------------------------------------------------*/

void AwsIot_FreeMqttOperation( void * ptr )
{
    /* Return the in-use MQTT operation. */
    AwsIotStaticMemory_ReturnInUse( ptr,
                                    _pMqttOperations,
                                    _pInUseMqttOperations,
                                    AWS_IOT_MQTT_MAX_IN_PROGRESS_OPERATIONS,
                                    sizeof( _mqttOperation_t ) );
}

/*-----------------------------------------------------------*/

void * AwsIot_MallocMqttTimerEvent( size_t size )
{
    int freeIndex = -1;
    void * pNewTimerEvent = NULL;

    /* Check size argument. */
    if( size == sizeof( _mqttTimerEvent_t ) )
    {
        /* Find a free MQTT timer event. */
        freeIndex = AwsIotStaticMemory_FindFree( _pInUseMqttTimerEvents,
                                                 AWS_IOT_MQTT_MAX_IN_PROGRESS_OPERATIONS );

        if( freeIndex != -1 )
        {
            pNewTimerEvent = &( _pMqttTimerEvents[ freeIndex ] );
        }
    }

    return pNewTimerEvent;
}

/*-----------------------------------------------------------*/

void AwsIot_FreeMqttTimerEvent( void * ptr )
{
    /* Return the in-use MQTT timer event. */
    AwsIotStaticMemory_ReturnInUse( ptr,
                                    _pMqttTimerEvents,
                                    _pInUseMqttTimerEvents,
                                    AWS_IOT_MQTT_MAX_IN_PROGRESS_OPERATIONS,
                                    sizeof( _mqttTimerEvent_t ) );
}

/*-----------------------------------------------------------*/

void * AwsIot_MallocMqttSubscription( size_t size )
{
    int freeIndex = -1;
    void * pNewSubscription = NULL;

    if( size <= _MQTT_SUBSCRIPTION_SIZE )
    {
        /* Get the index of a free MQTT subscription. */
        freeIndex = AwsIotStaticMemory_FindFree( _pInUseMqttSubscriptions,
                                                 AWS_IOT_MQTT_SUBSCRIPTIONS );

        if( freeIndex != -1 )
        {
            pNewSubscription = &( _pMqttSubscriptions[ freeIndex ][ 0 ] );
        }
    }

    return pNewSubscription;
}

/*-----------------------------------------------------------*/

void AwsIot_FreeMqttSubscription( void * ptr )
{
    /* Return the in-use MQTT subscription. */
    AwsIotStaticMemory_ReturnInUse( ptr,
                                    _pMqttSubscriptions,
                                    _pInUseMqttSubscriptions,
                                    AWS_IOT_MQTT_SUBSCRIPTIONS,
                                    _MQTT_SUBSCRIPTION_SIZE );
}

/*-----------------------------------------------------------*/

#endif
