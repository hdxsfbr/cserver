# Simple REST API for Contacts

This project implements a simple REST API to manage Contacts that supports CRUD operations.

## API Endpoints

- **Create Contact**: `POST /contacts`
- **Get All Contacts**: `GET /contacts`
- **Get Contact by ID**: `GET /contacts/{id}`
- **Update Contact**: `PUT /contacts/{id}`
- **Delete Contact**: `DELETE /contacts/{id}`

## Setup

To build the project, run:

```sh
make
```

## License

This project is licensed under the MIT License.

## Testing

To run the tests, compile the project and execute the `main` program in `src/main.c`, which demonstrates the CRUD operations on contacts.
- Implemented a simple REST API for managing contacts with CRUD operations in C.
- Created functions to handle Create, Read, Update, and Delete actions for contacts.
- Tested the functionality with a main program that demonstrates all the operations.
