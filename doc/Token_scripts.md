pragma solidity ^0.4.13;

contract Token {

    uint256 public totalSupply;

    function balanceOf(address _owner) constant returns (uint256 balance);

    function transfer(address _to, uint256 _value) returns (bool success);

    function transferFrom(address _from, address _to, uint256 _value) returns (bool success);

    function approve(address _spender, uint256 _value) returns (bool success);

    function allowance(address _owner, address _spender) constant returns (uint256 remaining);

    event Transfer(address indexed _from, address indexed _to, uint256 _value);

    event Approval(address indexed _owner, address indexed _spender, uint256 _value);
}


contract StandardToken is Token {

    function transfer(address _to, uint256 _value) returns (bool success) {

        if (balances[msg.sender] >= _value && _value > 0) {
            balances[msg.sender] -= _value;
            balances[_to] += _value;
            Transfer(msg.sender, _to, _value);
            return true;
        } else {
            return false;
        }
    }

    function transferFrom(address _from, address _to, uint256 _value) returns (bool success) {

        if (balances[_from] >= _value && allowed[_from][msg.sender] >= _value && _value > 0) {
            balances[_to] += _value;
            balances[_from] -= _value;
            allowed[_from][msg.sender] -= _value;
            Transfer(_from, _to, _value);
            return true;
        } else {
            return false;
        }
    }

    function balanceOf(address _owner) constant returns (uint256 balance) {
        return balances[_owner];
    }

    function approve(address _spender, uint256 _value) returns (bool success) {
        allowed[msg.sender][_spender] = _value;
        Approval(msg.sender, _spender, _value);
        return true;
    }

    function allowance(address _owner, address _spender) constant returns (uint256 remaining) {
        return allowed[_owner][_spender];
    }

    mapping (address => uint256) balances;
    mapping (address => mapping (address => uint256)) allowed;
}

contract LSRStandardToken is StandardToken {

    //if ether is sent to this address, send it back.
    function () {
        return;
    }

    //LSRToken 0.1 standard.
    string public version = 'I0.1';

    //fancy name: eg Luxcore
    string public name;

    //How many decimals to show.
    uint8 public decimals;

    //An identifier: eg LUX
    string public symbol;

    function LSRStandardToken(uint256 initialSupply, string tokenName, uint8 decimalUnits, string tokenSymbol) {

        // Give the creator all initial tokens
        balances[msg.sender] = initialSupply;

        // Update total supply
        totalSupply = initialSupply;

        // Set the name for display purposes
        name = tokenName;

        // Amount of decimals for display purposes
        decimals = decimalUnits;

        // Set the symbol for display purposes
        symbol = tokenSymbol;
    }

    //Approves and then calls contract
    function approveAndCall(address _spender, uint256 _value, bytes _extraData) returns (bool success) {

        allowed[msg.sender][_spender] = _value;
        Approval(msg.sender, _spender, _value);

        if(!_spender.call(bytes4(bytes32(sha3("receiveApproval(address,uint256,address,bytes)"))), msg.sender, _value, this, _extraData)) {
            return false;
        }
        return true;
    }
}
