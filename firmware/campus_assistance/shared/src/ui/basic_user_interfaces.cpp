#include "ca/ui/basic_user_interfaces.hpp"

namespace ca {

BasicStationUserInterface::BasicStationUserInterface(
    DestinationCatalog& destinations,
    StationRequestController& requests)
    : destinations_(destinations), requests_(requests), selectedIndex_(0),
      selectedDestinationId_(0), status_() {
  setStatus("Select destination");
  if (destinations_.count() > 0)
    selectedIndex_ = static_cast<uint16_t>(destinations_.count() - 1U);
  moveSelection(1);
}

void BasicStationUserInterface::onIntent(const UiIntent& intent) {
  switch (intent.type) {
  case UiIntentType::NEXT:
    moveSelection(1);
    break;
  case UiIntentType::PREVIOUS:
    moveSelection(-1);
    break;
  case UiIntentType::CONFIRM: {
    if (requests_.hasCurrentRequest()) {
      setStatus("Request active");
      break;
    }
    RequestId created = {};
    const Status submitted = requests_.submit(created);
    setStatus(submitted.ok() ? "Request queued" : "Submit failed");
    break;
  }
  case UiIntentType::CANCEL: {
    const Status cancelled = requests_.cancelCurrentRequest();
    setStatus(cancelled.ok() ? "Cancel queued" : "No active request");
    break;
  }
  default:
    break;
  }
}

UiViewModel BasicStationUserInterface::view() const {
  UiViewModel result = {};
  result.screenId = 0;
  char destinationLine[17] = {};
  copyLine(destinationLine, "Choose destination");
  Destination destination = {};
  if (selectedDestinationId_ != 0 &&
      destinations_.findById(selectedDestinationId_, destination)) {
    copyLine(destinationLine, destination.label);
    destinationLine[0] = '>';
    destinationLine[1] = ' ';
  }
  copyLine(result.line1, destinationLine);
  copyLine(result.line2,
           requests_.hasCurrentRequest() ? "Request active" : status_);
  return result;
}

void BasicStationUserInterface::moveSelection(int8_t direction) {
  const uint16_t count = destinations_.count();
  if (count == 0) {
    selectedDestinationId_ = 0;
    setStatus("No destinations");
    return;
  }

  uint16_t candidate = selectedIndex_;
  for (uint16_t checked = 0; checked < count; ++checked) {
    if (direction >= 0)
      candidate = static_cast<uint16_t>((candidate + 1U) % count);
    else
      candidate = candidate == 0 ? static_cast<uint16_t>(count - 1U)
                                 : static_cast<uint16_t>(candidate - 1U);

    Destination destination = {};
    if (!destinations_.getByIndex(candidate, destination) || !destination.enabled)
      continue;
    if (requests_.selectDestination(destination.id).ok()) {
      selectedIndex_ = candidate;
      selectedDestinationId_ = destination.id;
      setStatus("Press B to request");
      return;
    }
  }
  selectedDestinationId_ = 0;
  setStatus("No destinations");
}

void BasicStationUserInterface::setStatus(const char* message) {
  copyLine(status_, message);
}

void BasicStationUserInterface::copyLine(char output[17], const char* input) {
  uint8_t index = 0;
  while (index < 16 && input != 0 && input[index] != '\0') {
    output[index] = input[index];
    ++index;
  }
  while (index < 16)
    output[index++] = ' ';
  output[16] = '\0';
}

BasicCartUserInterface::BasicCartUserInterface() : view_() {
  setLine(0, "Accept: OFF");
  setLine(1, "Ready");
}

void BasicCartUserInterface::onIntent(const UiIntent& intent,
                                      const CartUiContext& context) {
  (void)intent;
  update(context);
}

void BasicCartUserInterface::update(const CartUiContext& context) {
  setLine(0, context.acceptanceMode == AcceptanceMode::ACCEPTING ? "Accept: ON"
                                                                 : "Accept: OFF");
  switch (context.serviceState) {
  case CartServiceState::IDLE:
    setLine(1, context.activeAssignment ? "Reconciling" : "Ready / Claim");
    break;
  case CartServiceState::CLAIM_PENDING:
    setLine(1, "Claim pending");
    break;
  case CartServiceState::SERVING:
    setLine(1, "Serving / Hold B");
    break;
  case CartServiceState::UNCERTAIN:
    setLine(1, "Check station");
    break;
  }
  view_.screenId = static_cast<uint8_t>(context.serviceState);
}

UiViewModel BasicCartUserInterface::view() const { return view_; }

void BasicCartUserInterface::setLine(uint8_t row, const char* message) {
  if (row > 1)
    return;
  char* output = row == 0 ? view_.line1 : view_.line2;
  uint8_t index = 0;
  while (index < 16 && message != 0 && message[index] != '\0') {
    output[index] = message[index];
    ++index;
  }
  while (index < 16)
    output[index++] = ' ';
  output[16] = '\0';
}

} // namespace ca
