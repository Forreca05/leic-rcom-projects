/**
 * @file download.h
 * @brief Definitions and interfaces for an FTP download application.
 *
 * This header file declares the data structures, constants, and function
 * prototypes used by an FTP client application that downloads files
 * from a remote server using the FTP protocol.
 */

#ifndef DOWNLOAD_H
#define DOWNLOAD_H

/**
 * @struct URL
 * @brief Structure that stores the components of an FTP URL.
 *
 * This structure holds all relevant fields extracted from an FTP URL,
 * including authentication credentials, server address, and file path
 * information. It is populated during the URL parsing stage and used
 * throughout the FTP communication process.
 */
typedef struct URL {
    /** Username for FTP authentication */
    char user[64];

    /** Password for FTP authentication */
    char password[64];

    /** Hostname of the FTP server */
    char host[128];

    /** Full path to the remote file on the FTP server */
    char path[64];

    /** Name of the file to be saved locally */
    char file[256];
} URL;

/** FTP control connection default port */
#define FTP_PORT 21

/** Maximum size of FTP command strings */
#define CMD_SIZE 128

/**
 * @name FTP Reply Codes
 * @brief FTP server response codes used for protocol validation.
 * @{
 */

/** Data connection already open; transfer starting */
#define CONNECTION_OPEN_CODE "125"

/** File status okay; opening data connection */
#define OPENING_CONNECTION_CODE "150"

/** Service ready for new user */
#define SERVICE_READY_CODE "220"

/** Service closing control connection */
#define QUIT_SUCCESS_CODE "221"

/** Requested file action completed successfully */
#define TRANSFER_COMPLETE_CODE "226"

/** Entering passive mode */
#define PASV_MODE_CODE "227"

/** User logged in, proceed */
#define LOGIN_SUCCESS_CODE "230"

/** Username accepted, password required */
#define USERNAME_OK_CODE "331"

/** @} */

/**
 * @brief Parses an FTP URL and extracts its components.
 *
 * This function processes an FTP URL provided as input and extracts
 * the username, password, host, path, and file name. If authentication
 * credentials are not specified, anonymous login is assumed.
 *
 * @param link String containing the FTP URL.
 * @param url Pointer to a URL structure where the parsed components
 *            will be stored.
 *
 * @return 0 on success, -1 if the URL is invalid or malformed.
 */
int parse(char *link, URL *url);

/**
 * @brief Creates and connects a TCP socket to a remote server.
 *
 * This function creates a TCP socket and establishes a connection
 * to the specified IP address and port.
 *
 * @param adr String containing the server IPv4 address.
 * @param port Port number to connect to.
 *
 * @return Socket file descriptor on success.
 *         The program terminates if socket creation or connection fails.
 */
int createsocket(const char *adr, int port);

#endif
