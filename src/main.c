#include <stdio.h>
#include "contacts.h"

int main() {
    // Create contacts
    Contact contact1 = {1, "John Doe", "john@example.com", "123-456-7890"};
    Contact contact2 = {2, "Jane Doe", "jane@example.com", "098-765-4321"};
    create_contact(contact1);
    create_contact(contact2);

    // Read contacts
    printf("Contacts List:\n");
    read_contacts();

    // Update a contact
    Contact updated_contact = {1, "John Smith", "john.smith@example.com", "123-111-1111"};
    update_contact(1, updated_contact);

    // Read contacts after update
    printf("\nContacts List after update:\n");
    read_contacts();

    // Delete a contact
    delete_contact(2);

    // Read contacts after deletion
    printf("\nContacts List after deletion:\n");
    read_contacts();

    return 0;
}