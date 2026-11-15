#ifndef ATM_PRESENTATION_RECEIPT_PRINTER_HPP
#define ATM_PRESENTATION_RECEIPT_PRINTER_HPP

#include "domain/journal_entry.hpp"

#include <ostream>

namespace atm::presentation {

    class IReceiptPrinter {
    public:
        virtual ~IReceiptPrinter() noexcept = default;

        virtual void print(const atm::domain::JournalEntry& entry) = 0;
    };

    class ConsoleReceiptPrinter : public IReceiptPrinter {
    private:
        std::ostream& out_;

    public:
        explicit ConsoleReceiptPrinter(std::ostream& out);

        void print(const atm::domain::JournalEntry& entry) override;
    };

} // namespace atm::presentation

#endif // ATM_PRESENTATION_RECEIPT_PRINTER_HPP
