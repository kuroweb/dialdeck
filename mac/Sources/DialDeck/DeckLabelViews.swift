import DialDeckKit
import SwiftUI

struct DeckSlotLabelPreview: View {
    let wire: String
    var compact = false

    var body: some View {
        switch DeckLabel.preview(wire, compact: compact) {
        case .text(let text):
            Text(text)
        case .symbol(let name):
            Image(systemName: name)
        case .symbols(let names):
            HStack(spacing: compact ? 1 : 3) {
                ForEach(names, id: \.self) { Image(systemName: $0) }
            }
        }
    }
}
