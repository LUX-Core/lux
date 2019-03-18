![LUX Logo](../src/qt/res/images/lux_logo_horizontal.png)

"FIRST OF ITS KIND"

Luxcore is GNU AGPLv3 licensed.

LUXGATE Perpetual & Futures contracts - Leverage & Margin trading.
==================================================================

Luxgate is a decentralized exchange, so all orders will be listed on LUX explorer and all transactions are stored in the LUX blockchain. 

## Perpetual & Futures contracts.

Futures are a derivative tool or agreement to buy or sell a primary asset at a predetermined price at a specified time in the future. With Luxgate you can trade using the futures contracts that cannot be delivered, which means that the difference between the perpetual contracts price and the spot price will be automatically calculated at the time specified in the contract.

## Leverage & Margin trading.

Financial leverage is the expression of the level of loan use in the total capital of the enterprise to increase the rate of equity return (ROE) or the income per ordinary share of the company.

Financial leverage refers to the combination of liabilities and owners' equity in regulating corporate financial policies. Financial leverage will be considerable in businesses with a higher proportion of liabilities than equity. Conversely, financial leverage will be low when the proportion of liabilities is smaller than the proportion of equity.

Financial leverage is both a tool to promote after-tax profits on an owner's equity and a tool to curb that increase. High profitability is the desire of the owners, in which financial leverage is a tool used by managers.


* Below is a basic example of using leverage, also known as simple leverage.


        A and B are currently trading LUX/USD in their locality.
        
        A used the amount of USD 100,000,000 to buy 10 LUX for USD 10,000,000 / LUX. The total amount A currently uses is 100,000,000 USD. He only uses his existing capital to do business without using financial leverage, not borrowing more.
        
        B also used the capital of USD 100,000,000 and borrowed more than USD 50,000,000 to buy 15 LUX for USD 10,000,000 / LUX. The total amount B currently uses is 150,000,000 USD. Currently, B borrowed more money from his friends, which is B's use of financial leverage. B is controlling 15 LUX with only USD 100,000,000.
        
        If LUX is favorable, the total ROE value of B and A increases by 25%, they sell then:
        
        * A profit USD 100,000,000 x 0.25 = USD 25,000,000, from the initial capital of USD 100,000,000. 25% profit
        
        * B earns USD 150,000,000 x 0.25 = USD 37,500,000, from it initial capital of USD 100,000,000. Profit of 37.5%.
        
        If LUX is unstable, the total ROE value of B and A decreases by 10%, they sell then:
        
        * A lost USD 100,000,000 x 10% = USD 10,000,000. Damage 10% of it capital.
        
        * B lost USD 150,000,000 x 10% = USD 15,000,000. Damage 15% of it capital.
        
        Through this, we see that when using financial leverage effectively, the profit margin is much higher than not using financial leverage. But it is also necessary to carefully  consider the business plans to minimize the risks that lead to losses.
        
    
* Formula to calculate financial leverage

        DFL = (% change in EPS) / (% change in EBIT)

        DFL is a leverage ratio that shows the effect of a specific debt on the company's earnings per share.

* Leverage
        
Luxgate offers leverage on operations with derivatives with the maximum of `x100 leverage`. `Note: high leverage increases trading risks`.

* Masternode Collateral System & Open Position

To open a position, you need to have a sufficient initial margin level which is holding by our `masternode collateral` function. It is a 100% `decentralised collateral system` that Luxcore specially implemented to serves as collateral for your order. It is calculated in proportion to the leverage you choose. 

Example:

    Initial margin for leverage x2 is 50% of the order amount; initial margin for leverage x10 is 10% of the order amount; for leverage x 25 you need to deposit only 4% of the order amount. After your order is executed, 30% of the initial margin will be transferred to you Margin Balance. This amount will be automatically used to maintain your positions, or you can use it to place new orders or transfer it to your account.

* Liquidation

You need to have a sufficient level of maintenance margin balance for your current position, otherwise, your position will be liquidated

    70% of the initial margin for your postion
    
    Margin Balance, including 30% of the initial margin for your position

    If your position exceeds maintenance margin, it will be liquidated. 
    
* Cross Margin
 
Luxgate also has cross margin method. This means that the maintenance margin of your Margin Balance will be shared among all your open positions. Therefore, when a trader has several open positions, the maintenance margin also includes:

    Unrealized positive PNL on all open positions

Example:

     (.....)
 
### Margin Balance

Margin balances are designed to maintain existing positions with leverage. Margin balances automatically maintain positions close to liquidation.

### Liquidation Process

If your total balance is not enough to maintain your position, they can be liquidated. The Fair price marking system is used to calculate the liquidation price. It allows avoiding spurious liquidation when price exceptions occur due to market manipulation. If traders cannot meet their maintenance requirements, they will be liquidated.

### Fees

To record information about opening an order and add information about a completed transaction to the LUX blockchain, you need to spend a particular amount of TX fee.


* Luxgate fees, Makers and Takers fees.

        Fee for makers is 0% + blockchain fee (TX fee) for placing an order.

        Fee for takers is 0,1% + blockchain fee (TX fee) for placing an order. 
        
        TX fee is used for all the transaction within the LUX blockchain system.
