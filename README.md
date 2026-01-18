# Simple REST API for Contacts

This project implements a small in-memory REST API to manage contacts with CRUD operations.
Contacts are stored in memory only (no persistence) and IDs are assigned by the server.

## API Endpoints

- **Create Contact**: `POST /contacts` with JSON body `{ "name": "...", "email": "...", "phone": "..." }`
- **Get All Contacts**: `GET /contacts`
- **Get Contact by ID**: `GET /contacts/{id}`
- **Update Contact**: `PUT /contacts/{id}` with JSON body `{ "name": "...", "email": "...", "phone": "..." }`
- **Delete Contact**: `DELETE /contacts/{id}`

## Notes

- Maximum of 100 contacts per process.
- All fields (`name`, `email`, `phone`) are required on create and update.

## Setup

To build the project, run:

```sh
make
```

The server is implemented with a small POSIX socket loop and keeps all data in memory.

## License

This project is licensed under the MIT License.

## Run

Start the server in one terminal:

```sh
make && ./main
```

It listens on `http://localhost:8000`.

## Quick Test

Then exercise the API in another terminal:

```sh
curl -s -X POST http://localhost:8000/contacts \
  -H 'Content-Type: application/json' \
  -d '{"name":"Ada Lovelace","email":"ada@example.com","phone":"555-0100"}'

curl -s http://localhost:8000/contacts

curl -s http://localhost:8000/contacts/1

curl -s -X PUT http://localhost:8000/contacts/1 \
  -H 'Content-Type: application/json' \
  -d '{"name":"Ada Lovelace","email":"ada@newmail.com","phone":"555-0199"}'

curl -i -X DELETE http://localhost:8000/contacts/1
```
